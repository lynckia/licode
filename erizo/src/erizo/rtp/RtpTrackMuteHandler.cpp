#include "rtp/RtpTrackMuteHandler.h"
#include "./MediaDefinitions.h"
#include "./MediaStream.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpTrackMuteHandler, "rtp.RtpTrackMuteHandler");

RtpTrackMuteHandler::RtpTrackMuteHandler() : audio_info_{"audio"}, video_info_{"video"}, stream_{nullptr} {}

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
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  uint16_t offset = info.seq_num_offset;
  if (offset > 0) {
    char* buf = packet->data;
    char* report_pointer = buf;
    int rtcp_length = 0;
    int total_length = 0;
    do {
      report_pointer += rtcp_length;
      chead = reinterpret_cast<RtcpHeader*>(report_pointer);
      rtcp_length = (ntohs(chead->length) + 1) * 4;
      total_length += rtcp_length;
      switch (chead->packettype) {
        case RTCP_Receiver_PT:
          if ((chead->getHighestSeqnum() + offset) < chead->getHighestSeqnum()) {
            // The seqNo adjustment causes a wraparound, add to cycles
            chead->setSeqnumCycles(chead->getSeqnumCycles() + 1);
          }
          chead->setHighestSeqnum(chead->getHighestSeqnum() + offset);

          break;
        case RTCP_RTP_Feedback_PT:
          chead->setNackPid(chead->getNackPid() + offset);
          break;
        default:
          break;
      }
    } while (total_length < packet->length);
  }
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
  info->last_original_seq_num = rtp_header->getSeqNumber();
  if (info->last_sent_seq_num == -1) {
    info->last_sent_seq_num = info->last_original_seq_num;
  }
  if (info->mute_is_active) {
    if (packet->is_keyframe) {
      if (info->unmute_requested) {
        ELOG_DEBUG("%s message: Keyframe arrived - unmuting video", stream_->toLog());
        info->mute_is_active = false;
        info->unmute_requested = false;
      } else {
        ELOG_DEBUG("%s message: video muted - maybe transforming into black keyframe", stream_->toLog());
        if (packet->codec == "VP8") {
          packet = transformIntoBlackKeyframePacket(packet);
        } else {
          ELOG_WARN("%s cannot generate keyframe packet is not available for codec %s",
              stream_->toLog(), packet->codec);
          return;
        }
      }
      updateOffset(info);
    } else {
      return;
    }
  }
  uint16_t offset = info->seq_num_offset;
  info->last_sent_seq_num = info->last_original_seq_num - offset;
  if (offset > 0) {
    setPacketSeqNumber(packet, info->last_sent_seq_num);
  }
  ctx->fireWrite(std::move(packet));
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
      ELOG_DEBUG("%s message: Unmute requested for video, original_seq_num: %u, last_sent_seq_num: %u, offset: %u",
          stream_->toLog(), info->last_original_seq_num, info->last_sent_seq_num, info->seq_num_offset);
    } else {
      info->mute_is_active = active;
      updateOffset(info);
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

void RtpTrackMuteHandler::updateOffset(TrackMuteInfo *info) {
  info->seq_num_offset = info->last_original_seq_num - info->last_sent_seq_num;
}


std::shared_ptr<DataPacket> RtpTrackMuteHandler::transformIntoBlackKeyframePacket
  (std::shared_ptr<DataPacket> packet) {
      auto keyframe_packet = RtpUtils::makeVP8BlackKeyframePacket(packet);
      return keyframe_packet;
  }
}  // namespace erizo
