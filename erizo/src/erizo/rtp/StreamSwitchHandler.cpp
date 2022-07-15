#include "rtp/StreamSwitchHandler.h"

#include <string>

#include "./MediaDefinitions.h"
#include "./MediaStream.h"
#include "rtp/RtpUtils.h"
#include "rtp/RtpVP8Parser.h"

namespace erizo {

constexpr uint64_t kPliPeriodMs = 1000;

DEFINE_LOGGER(StreamSwitchHandler, "rtp.StreamSwitchHandler");

StreamSwitchHandler::StreamSwitchHandler(std::shared_ptr<erizo::Clock> the_clock)
  : stream_{nullptr}, video_ssrc_{0}, generate_video_{false}, generated_seq_number_{0},
  clock_{the_clock}, fir_seq_number_{0}, enable_plis_{true}, plis_scheduled_{false} {}

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
    uint32_t now = getNow();
    bool is_connected = media_stream_switch_event->is_connected;
    bool has_video = media_stream_switch_event->has_video;
    ELOG_DEBUG("Sending PLI? %u %u %s", is_connected, has_video, stream_->getLabel());
    if (is_connected && has_video) {
      enable_plis_ = true;
      sendPLI();
      schedulePLI();
    } else {
      enable_plis_ = false;
    }
    std::for_each(state_map_.begin(), state_map_.end(),
      [this, now, is_connected] (std::pair<const unsigned int, std::shared_ptr<TrackState>> state_pair) {
        auto state = state_pair.second;
        if (is_connected) {
          ELOG_DEBUG("Mark as switched SSRC %u %s", state_pair.first, stream_->getLabel());
          state->switched = true;
          state->keyframe_received = false;
          state->frame_received = false;
          state->switch_called_at = now;
          if (state->last_timestamp_sent_at > 0) {
            uint32_t time_with_silence = now - state->last_timestamp_sent_at;

            if (state->clock_rate > 0) {
              state->time_with_silence = (1 + time_with_silence) * (state->clock_rate / 1000);
              ELOG_DEBUG("Adding silence of %d, %d, %d", time_with_silence, state->clock_rate, state->time_with_silence);
            }
          }
        } else {
          if (state->last_packet) {
            if (state->clock_rate > 0) {
              sendBlackKeyframe(state->last_packet, 1, state->clock_rate, state);
            }
            state->last_packet.reset();
          }
          state->last_timestamp_sent_at = now;
          state->time_with_silence = 0;
        }
      });
  }
}

void StreamSwitchHandler::sendBlackKeyframe(std::shared_ptr<DataPacket> incoming_packet, int additional,
    uint32_t clock_rate, const std::shared_ptr<TrackState> &state) {
  if (incoming_packet->codec == "VP8") {
    auto packet = RtpUtils::makeVP8BlackKeyframePacket(incoming_packet);
    packet->is_keyframe = true;
    packet->compatible_temporal_layers = {0, 1, 2};

    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    packet->picture_id = state->last_picture_id_received + additional;
    packet->tl0_pic_idx = state->last_tl0_pic_idx_received + additional;
    rtp_header->setSeqNumber(state->last_sequence_number_received + additional);
    // We use a big margin for the timestamp to make sure that Chrome renders it. We were using just
    // `additional` and Chrome usually dropped it because it was too close to the previous frame.
    rtp_header->setTimestamp(state->last_timestamp_received + additional * (clock_rate / 1000));
    ELOG_DEBUG("Sending keyframe before switch");
    write(getContext(), packet);
  }
}

void StreamSwitchHandler::sendPLI() {
  if (enable_plis_) {
    ELOG_DEBUG("message: Sending PLI");
    getContext()->fireRead(RtpUtils::createPLI(stream_->getVideoSinkSSRC(), stream_->getVideoSourceSSRC()));
  }
}

void StreamSwitchHandler::schedulePLI() {
  if (plis_scheduled_) {
    return;
  }
  plis_scheduled_ = true;
  std::weak_ptr<StreamSwitchHandler> weak_this = shared_from_this();
  stream_->getWorker()->scheduleEvery([weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      bool pli_needed = false;
      std::for_each(this_ptr->state_map_.begin(), this_ptr->state_map_.end(),
          [&pli_needed, this_ptr] (std::pair<const unsigned int, std::shared_ptr<TrackState>> state_pair) {
        if (!state_pair.second->keyframe_received &&
            state_pair.second->frame_received &&
            this_ptr->enable_plis_) {
          pli_needed = true;
        }
      });
      if (pli_needed) {
        this_ptr->sendPLI();
        return true;
      } else {
        this_ptr->plis_scheduled_ = false;
        return false;
      }
    }
    return false;
  }, std::chrono::milliseconds(kPliPeriodMs));
}

void StreamSwitchHandler::handleFeedbackPackets(const std::shared_ptr<DataPacket> &packet) {
  bool block_packet = false;
  RtpUtils::forEachRtcpBlock(packet, [this, &block_packet](RtcpHeader *chead) {
    if (chead->packettype == RTCP_PS_Feedback_PT &&
          (chead->getBlockCount() == RTCP_PLI_FMT ||
           chead->getBlockCount() == RTCP_SLI_FMT ||
           chead->getBlockCount() == RTCP_FIR_FMT)) {
      uint32_t ssrc = chead->getSourceSSRC();
      ELOG_WARN("PLI through StreamSwitchHandler!%s", stream_->getLabel());
      std::shared_ptr<TrackState> state = getStateForSsrc(ssrc, true);
      if ((state && !state->frame_received) || !enable_plis_) {
        block_packet = true;
      }
    }
  });
  if (!block_packet) {
    getContext()->fireRead(std::move(packet));
  } else {
    ELOG_DEBUG("message: Blocking PLI %s", stream_->getLabel());
  }
}

void StreamSwitchHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (chead->isFeedback()) {
    handleFeedbackPackets(packet);
    return;
  }
  ctx->fireRead(std::move(packet));
}

uint32_t StreamSwitchHandler::getNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
          clock_->now().time_since_epoch())
          .count();
}

void StreamSwitchHandler::storeLastPacket(const std::shared_ptr<TrackState> &state,
    const std::shared_ptr<DataPacket> &packet) {
  state->last_packet = std::make_shared<DataPacket>(*packet);
  state->last_packet->picture_id = packet->picture_id;
  state->last_packet->tl0_pic_idx = packet->tl0_pic_idx;
  state->last_packet->rid = packet->rid;
  state->last_packet->mid = packet->mid;
  state->last_packet->priority = packet->priority;
  state->last_packet->received_time_ms = packet->received_time_ms;
  state->last_packet->compatible_spatial_layers = packet->compatible_spatial_layers;
  state->last_packet->compatible_temporal_layers = packet->compatible_temporal_layers;
  state->last_packet->ending_of_layer_frame = packet->ending_of_layer_frame;
  state->last_packet->codec = packet->codec;
  state->last_packet->clock_rate = packet->clock_rate;
  state->last_packet->is_padding = packet->is_padding;
}

void StreamSwitchHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp()) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    uint32_t ssrc = rtp_header->getSSRC();
    uint16_t sequence_number = rtp_header->getSeqNumber();
    uint32_t new_timestamp = rtp_header->getTimestamp();
    uint16_t picture_id = 0;
    uint8_t tl0_pic_idx = 0;
    uint32_t now = getNow();
    if (packet->type == VIDEO_PACKET) {
      picture_id = packet->picture_id;
      tl0_pic_idx = packet->tl0_pic_idx;
    }
    std::shared_ptr<TrackState> state = getStateForSsrc(ssrc, true);

    // Flag first frame received
    state->frame_received = true;

    // Convert frames to keyframes until we receive the first real keyframe
    if (!state->keyframe_received && packet->type == VIDEO_PACKET) {
      if (packet->is_keyframe) {
        uint32_t time_to_receive_first_keyframe = 0;
        if (state->switch_called_at > 0) {
          time_to_receive_first_keyframe = now - state->switch_called_at;
        }
        state->keyframe_received = true;
        ELOG_INFO("Switching stream, keyframe received, ssrc: %u, time: %u, audio: 0", ssrc, time_to_receive_first_keyframe);
      } else {
        // Convert to keyframes until we receive a new keyframe
        packet = RtpUtils::makeVP8BlackKeyframePacket(packet);
        packet->compatible_temporal_layers = {0, 1, 2};
        rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
        if (!plis_scheduled_) {
          schedulePLI();
        }
      }
    }
    if (!state->keyframe_received && packet->type == AUDIO_PACKET) {
      state->keyframe_received = true;
    }

    // Reset translators and calculate offsets
    if (state->switched) {
      uint32_t time_to_finish_stream_switch = 0;
      if (state->switch_called_at > 0) {
        time_to_finish_stream_switch = now - state->switch_called_at;
      }
      ELOG_INFO("Switching stream, ssrc: %u, time: %u, audio: %u", ssrc, time_to_finish_stream_switch, packet->type);
      state->switched = false;
      state->sequence_number_translator.reset();
      // state->picture_id_translator.reset();
      state->picture_id_offset = state->last_picture_id_sent - picture_id + 1;
      state->timestamp_offset = state->last_timestamp_sent - new_timestamp + state->time_with_silence;
      state->tl0_pic_idx_offset = state->last_tl0_pic_idx_sent - tl0_pic_idx + 1;
      state->time_with_silence = 0;
      state->last_timestamp_sent_at = 0;
    }

    // Translations (sequence number, timestamp, and picture_id and tl0_pic_idx in video)
    SequenceNumber sequence_number_info = state->sequence_number_translator.get(sequence_number, false);
    if (sequence_number_info.type != SequenceNumberType::Valid) {
      ELOG_DEBUG("message: Dropping packet due to wrong sequence number translation, sequenceNumber: %u", sequence_number);
      return;
    }
    rtp_header->setSeqNumber(sequence_number_info.output);

    uint32_t timestamp = new_timestamp + state->timestamp_offset;
    rtp_header->setTimestamp(timestamp);

    state->clock_rate = packet->clock_rate;

    if (packet->type == VIDEO_PACKET) {
      // SequenceNumber picture_id_info = state->picture_id_translator.get(picture_id, false);
      // if (picture_id_info.type != SequenceNumberType::Valid) {
      //   ELOG_DEBUG("message: Dropping packet due to wrong picture id translation, pictureId: %u, lastReceived: %u, lastSent: %u", picture_id, state->last_picture_id_received, state->last_picture_id_sent);
      //   return;
      // }

      packet->picture_id = (picture_id + state->picture_id_offset) & 0x7FFF;
      updatePictureID(packet, packet->picture_id);
      packet->tl0_pic_idx = tl0_pic_idx + state->tl0_pic_idx_offset;
      updateTL0PicIdx(packet, packet->tl0_pic_idx);
    }
    if (!state->initialized) {
      state->last_sequence_number_sent = sequence_number_info.output;
      state->last_sequence_number_received = sequence_number;
      state->initialized = true;
    }
    // Save references if packet is the highest sequence number we received
    bool is_latest_sequence_number = !RtpUtils::sequenceNumberLessThan(sequence_number_info.output, state->last_sequence_number_sent);
    if (is_latest_sequence_number) {
      state->last_sequence_number_sent = sequence_number_info.output;
      state->last_sequence_number_received = sequence_number;
      state->last_timestamp_sent = timestamp;
      state->last_timestamp_received = new_timestamp;

      if (packet->type == VIDEO_PACKET) {
        state->last_picture_id_sent = packet->picture_id;
        state->last_picture_id_received = picture_id;
        if (packet->belongsToTemporalLayer(0) && packet->is_keyframe) {
          storeLastPacket(state, packet);
        }
      }
    }
    if (!state->initialized) {
      state->last_tl0_pic_idx_sent = packet->tl0_pic_idx;
      state->last_tl0_pic_idx_received = tl0_pic_idx;
    }
    if (packet->type == VIDEO_PACKET && packet->belongsToTemporalLayer(0)) {
      if (RtpUtils::numberLessThan(state->last_tl0_pic_idx_sent, packet->tl0_pic_idx, 8)) {
        state->last_tl0_pic_idx_sent =  packet->tl0_pic_idx;
        state->last_tl0_pic_idx_received = tl0_pic_idx;
      }
    }
    if (!state->initialized) {
      state->initialized = true;
    }
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

void StreamSwitchHandler::updatePictureID(const std::shared_ptr<DataPacket> &packet, int new_picture_id) {
  if (packet->codec == "VP8") {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    packet->picture_id = new_picture_id;
    RtpVP8Parser::setVP8PictureID(start_buffer, packet->length - rtp_header->getHeaderLength(), new_picture_id);
  }
}

void StreamSwitchHandler::updateTL0PicIdx(const std::shared_ptr<DataPacket> &packet, uint8_t new_tl0_pic_idx) {
  if (packet->codec == "VP8") {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    packet->tl0_pic_idx = new_tl0_pic_idx;
    RtpVP8Parser::setVP8TL0PicIdx(start_buffer, packet->length - rtp_header->getHeaderLength(), new_tl0_pic_idx);
  }
}
}  // namespace erizo
