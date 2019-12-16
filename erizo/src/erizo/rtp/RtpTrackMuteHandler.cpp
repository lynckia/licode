#include "rtp/RtpTrackMuteHandler.h"
#include "./MediaDefinitions.h"
#include "./MediaStream.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpTrackMuteHandler, "rtp.RtpTrackMuteHandler");

RtpTrackMuteHandler::RtpTrackMuteHandler(std::shared_ptr<erizo::Clock> the_clock) : audio_info_{"audio"},
  video_info_{"video"}, clock_{the_clock}, stream_{nullptr} {}

void RtpTrackMuteHandler::enable() {
}

void RtpTrackMuteHandler::disable() {
}

void RtpTrackMuteHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
  }
  muteTrack(&audio_info_, stream_->isAudioMuted());
  muteTrack(&video_info_, stream_->isVideoMuted());
}

void RtpTrackMuteHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);

  if (stream_->getAudioSinkSSRC() == chead->getSourceSSRC()) {
    handleFeedback(audio_info_, packet);
  } else if (stream_->getVideoSinkSSRC() == chead->getSourceSSRC()) {
    handleFeedback(video_info_, packet);
  }

  ctx->fireRead(std::move(packet));
}

void RtpTrackMuteHandler::handleFeedback(const TrackMuteInfo &info, const std::shared_ptr<DataPacket> &packet) {
  RtpUtils::forEachRtcpBlock(packet, [info](RtcpHeader *chead) {
    switch (chead->packettype) {
      case RTCP_Receiver_PT:
        {
          uint16_t incoming_seq_num = chead->getHighestSeqnum();
          SequenceNumber input_seq_num = info.translator.reverse(incoming_seq_num);
          if (input_seq_num.type != SequenceNumberType::Valid) {
            break;
          }
          if (RtpUtils::sequenceNumberLessThan(input_seq_num.input, incoming_seq_num)) {
            chead->setSeqnumCycles(chead->getSeqnumCycles() - 1);
          }

          chead->setHighestSeqnum(input_seq_num.input);
          break;
        }
      case RTCP_RTP_Feedback_PT:
        {
          SequenceNumber input_seq_num = info.translator.reverse(chead->getNackPid());
          if (input_seq_num.type == SequenceNumberType::Valid) {
            chead->setNackPid(input_seq_num.input);
          }
          break;
        }
      default:
        break;
    }
  });
}

void RtpTrackMuteHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);
  if (rtcp_header->isRtcp()) {
    ctx->fireWrite(std::move(packet));
  } else if (packet->type == AUDIO_PACKET) {
    handlePacket(ctx, &audio_info_, std::move(packet));
  } else if (packet->type == VIDEO_PACKET) {
    handlePacket(ctx, &video_info_, std::move(packet));
  } else {
    ctx->fireWrite(std::move(packet));
  }
}

void RtpTrackMuteHandler::handlePacket(Context *ctx, TrackMuteInfo *info, std::shared_ptr<DataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  time_point now = clock_->now();
  bool should_skip_packet = false;
  if (info->mute_is_active) {
    if (info->label == "video") {
      if (packet->is_keyframe && info->unmute_requested) {
        ELOG_DEBUG("%s message: Keyframe arrived - unmuting video", stream_->toLog());
        info->mute_is_active = false;
        info->unmute_requested = false;
        last_keyframe_sent_time_ = now;
      } else if (packet->is_keyframe ||
          (now - last_keyframe_sent_time_) > kMuteVideoKeyframeTimeout) {
        ELOG_DEBUG("message: Will create Keyframe last_keyframe, time: %u, is_keyframe: %u",
            now - last_keyframe_sent_time_, packet->is_keyframe);
        if (packet->codec == "VP8") {
          packet = transformIntoBlackKeyframePacket(packet);
          last_keyframe_sent_time_ = now;
        } else {
          ELOG_INFO("%s message: cannot generate keyframe packet is not available for codec %s",
              stream_->toLog(), packet->codec);
          should_skip_packet = true;
        }
      } else {
        should_skip_packet = true;
      }
    } else {
      should_skip_packet = true;
    }
  }
  uint16_t packet_seq_num = rtp_header->getSeqNumber();
  maybeUpdateHighestSeqNum(info, packet_seq_num);
  SequenceNumber sequence_number_info = info->translator.get(packet_seq_num, should_skip_packet);
  if (!should_skip_packet) {
    setPacketSeqNumber(packet, sequence_number_info.output);
    ctx->fireWrite(std::move(packet));
  }
}

void RtpTrackMuteHandler::maybeUpdateHighestSeqNum(TrackMuteInfo *info, uint16_t seq_num) {
  if (RtpUtils::sequenceNumberLessThan(info->highest_seq_num, seq_num) || !info->highest_seq_num_initialized) {
    info->highest_seq_num = seq_num;
    info->highest_seq_num_initialized = true;
  }
}

void RtpTrackMuteHandler::muteAudio(bool active) {
  muteTrack(&audio_info_, active);
}

void RtpTrackMuteHandler::muteVideo(bool active) {
  muteTrack(&video_info_, active);
}

void RtpTrackMuteHandler::muteTrack(TrackMuteInfo *info, bool active) {
  if (info->mute_is_active == active) {
    return;
  }
  ELOG_INFO("%s message: muteTrack, label:%s, active: %d", stream_->toLog(), info->label.c_str(), active);
  if (!active) {
    if (info->label == "video") {
      info->unmute_requested = true;
      getContext()->fireRead(RtpUtils::createPLI(stream_->getVideoSinkSSRC(), stream_->getVideoSourceSSRC()));
      ELOG_DEBUG("%s message: Unmute requested for video",
          stream_->toLog());
    } else {
      info->mute_is_active = active;
    }
  } else {
    info->mute_is_active = active;
  }
}

inline void RtpTrackMuteHandler::setPacketSeqNumber(std::shared_ptr<DataPacket> packet, uint16_t seq_number) {
  RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    return;
  }
  head->setSeqNumber(seq_number);
}


std::shared_ptr<DataPacket> RtpTrackMuteHandler::transformIntoBlackKeyframePacket
  (std::shared_ptr<DataPacket> packet) {
      auto keyframe_packet = RtpUtils::makeVP8BlackKeyframePacket(packet);
      return keyframe_packet;
  }
}  // namespace erizo
