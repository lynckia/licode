#include "rtp/StreamSwitchHandler.h"

#include <string>

#include "./MediaDefinitions.h"
#include "./MediaStream.h"
#include "rtp/RtpUtils.h"
#include "rtp/RtpVP8Parser.h"

namespace erizo {

constexpr uint64_t kKeyframePeriodMs = 3000;

DEFINE_LOGGER(StreamSwitchHandler, "rtp.StreamSwitchHandler");

StreamSwitchHandler::StreamSwitchHandler(std::shared_ptr<erizo::Clock> the_clock)
  : stream_{nullptr}, video_ssrc_{0}, generate_video_{false}, generated_seq_number_{0}, clock_{the_clock} {}

void StreamSwitchHandler::enable() {}

void StreamSwitchHandler::disable() {}

void StreamSwitchHandler::notifyUpdate() {
  if (stream_) {
    return;
  }
  auto pipeline = getContext()->getPipelineShared();
  stream_ = pipeline->getService<MediaStream>().get();
  video_ssrc_ = stream_->getVideoSourceSSRC();
  std::shared_ptr<SdpInfo> remote_sdp = stream_->getRemoteSdpInfo();
  RtpMap *pt = remote_sdp->getCodecByName("VP8");
  if (!pt) {
    pt = remote_sdp->getCodecByName("VP9");
  }
  if (!pt) {
    pt = remote_sdp->getCodecByName("H264");
  }
  if (pt) {
    video_pt_ = pt->payload_type;
    video_codec_name_ = pt->encoding_name;
    video_clock_rate_ = pt->clock_rate;
  }
}

void StreamSwitchHandler::notifyEvent(MediaEventPtr event) {
  if (event->getType() == "MediaStreamSwitchEvent") {
    auto media_stream_switch_event = std::static_pointer_cast<MediaStreamSwitchEvent>(event);
    std::for_each(state_map_.begin(), state_map_.end(),
      [this] (std::pair<const unsigned int, std::shared_ptr<TrackState>> state_pair) {
        ELOG_DEBUG("Mark as switched SSRC %u", state_pair.first);
        if (state_pair.second->last_packet) {
          sendBlackKeyframe(state_pair.second->last_packet);
          state_pair.second->last_packet.reset();
        }
        state_pair.second->switched = true;
      });
    // DeliverEvent to show bitrate per layer.
  }
}

void StreamSwitchHandler::sendBlackKeyframe(std::shared_ptr<DataPacket> incoming_packet) {
  if (incoming_packet->codec == "VP8") {
    auto packet = RtpUtils::makeVP8BlackKeyframePacket(incoming_packet);
    // packet->is_keyframe = false;
    packet->picture_id++;
    packet->tl0_pic_idx++;
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    uint16_t sequence_number = rtp_header->getSeqNumber();
    uint32_t new_timestamp = rtp_header->getTimestamp();
    rtp_header->setSeqNumber(sequence_number + 1);
    rtp_header->setTimestamp(new_timestamp + 1);
    ELOG_WARN("Sending keyframe before switch");
    // write(getContext(), packet);
  }
}

void StreamSwitchHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireRead(std::move(packet));
}

uint32_t StreamSwitchHandler::getNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
          clock_->now().time_since_epoch())
          .count();
}

void StreamSwitchHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp()) {
    uint32_t now = getNow();
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    uint32_t ssrc = rtp_header->getSSRC();
    uint16_t sequence_number = rtp_header->getSeqNumber();
    uint32_t new_timestamp = rtp_header->getTimestamp();
    int picture_id = 0;
    uint8_t tl0_pic_idx = 0;
    packetType type = packet->type;
    if (type == VIDEO_PACKET) {
      picture_id = packet->picture_id;
      tl0_pic_idx = packet->tl0_pic_idx;
    }
    std::shared_ptr<TrackState> state = getStateForSsrc(ssrc, true);
    if (state->switched) {
      ELOG_DEBUG("Switching SSRC %u", ssrc);
      state->switched = false;
      state->sequence_number_translator.reset();
      state->picture_id_translator.reset();
      if (state->last_timestamp_sent > 0) {
        uint32_t time_with_silence = now - state->last_timestamp_sent_at;
        ELOG_WARN("Adding silence of %d, %d", time_with_silence, state->clock_rate);
        if (state->clock_rate > 0) {
          time_with_silence = time_with_silence * state->clock_rate / 1000;
        }
        state->timestamp_offset = state->last_timestamp_sent - new_timestamp + 1 + time_with_silence;
      }
      if (state->last_tl0_pic_idx_sent > 0) {
        state->tl0_pic_idx_offset = state->last_tl0_pic_idx_sent - tl0_pic_idx + 1;
      }
    }
    // ELOG_DEBUG("Incoming packet, ssrc: %u, sn: %u, ts: %u, pid: %d, tl0pic: %d",
    //   ssrc, sequence_number, new_timestamp, picture_id, tl0_pic_idx);

    SequenceNumber sequence_number_info = state->sequence_number_translator.get(sequence_number, false);
    if (sequence_number_info.type != SequenceNumberType::Valid) {
      return;
    }

    if (type == VIDEO_PACKET) {
      state->last_packet = std::make_shared<DataPacket>(*packet);
      state->last_packet->picture_id = packet->picture_id;
      state->last_packet->tl0_pic_idx = packet->tl0_pic_idx;
    }

    rtp_header->setSeqNumber(sequence_number_info.output);

    state->last_timestamp_sent = new_timestamp + state->timestamp_offset;
    state->last_timestamp_sent_at = now;
    state->clock_rate = packet->clock_rate;
    rtp_header->setTimestamp(state->last_timestamp_sent);

    if (type == VIDEO_PACKET) {
      SequenceNumber picture_id_info = state->picture_id_translator.get(picture_id, false);
      if (picture_id_info.type != SequenceNumberType::Valid) {
        return;
      }

      packet->picture_id = picture_id_info.output & 0x7FFF;

      uint8_t tl0_pic_idx_sent = tl0_pic_idx + state->tl0_pic_idx_offset;
      state->last_tl0_pic_idx_sent = RtpUtils::numberLessThan(state->last_tl0_pic_idx_sent, tl0_pic_idx_sent, 8) ?
        tl0_pic_idx_sent : state->last_tl0_pic_idx_sent;
      packet->tl0_pic_idx = tl0_pic_idx_sent;
      tl0_pic_idx = tl0_pic_idx_sent;
      picture_id = picture_id_info.output;
    }
    ELOG_DEBUG("         packet, ssrc: %u, sn: %u, ts: %u, pid: %d, tl0pic: %d, keyframe: %d",
        ssrc, sequence_number_info.output, state->last_timestamp_sent, picture_id, tl0_pic_idx, packet->is_keyframe);
  }
  ctx->fireWrite(std::move(packet));
}

std::shared_ptr<TrackState> StreamSwitchHandler::getStateForSsrc(uint32_t ssrc,
    bool should_create) {
  auto state_it = state_map_.find(ssrc);
  std::shared_ptr<TrackState> state;
  if (state_it != state_map_.end()) {
      // ELOG_DEBUG("Found Translator for %u, %s", ssrc, stream_->toLog());
      state = state_it->second;
  } else if (should_create) {
      ELOG_DEBUG("message: no Translator found creating a new one, ssrc: %u, %s", ssrc, stream_->toLog());
      state = std::make_shared<TrackState>();
      state_map_[ssrc] = state;
  }
  return state;
}
}  // namespace erizo
