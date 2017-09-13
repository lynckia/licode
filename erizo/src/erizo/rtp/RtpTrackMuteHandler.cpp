#include "rtp/RtpTrackMuteHandler.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpTrackMuteHandler, "rtp.RtpTrackMuteHandler");

RtpTrackMuteHandler::RtpTrackMuteHandler() : audio_info_{"audio"}, video_info_{"video"}, connection_{nullptr} {}

void RtpTrackMuteHandler::enable() {
}

void RtpTrackMuteHandler::disable() {
}

void RtpTrackMuteHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
  }
  muteTrack(&audio_info_, connection_->isAudioMuted());
  muteTrack(&video_info_, connection_->isVideoMuted());
}

void RtpTrackMuteHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);

  if (connection_->getAudioSinkSSRC() == chead->getSourceSSRC()) {
    handleFeedback(audio_info_, packet);
  } else if (connection_->getVideoSinkSSRC() == chead->getSourceSSRC()) {
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
  uint16_t offset = info->seq_num_offset;
  info->last_original_seq_num = rtp_header->getSeqNumber();
  if (!info->mute_is_active) {
    info->last_sent_seq_num = info->last_original_seq_num - offset;
    if (offset > 0) {
      setPacketSeqNumber(packet, info->last_sent_seq_num);
    }
    ctx->fireWrite(std::move(packet));
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
  info->mute_is_active = active;
  ELOG_INFO("%s message: Mute %s, active: %d", info->label.c_str(), connection_->toLog(), active);
  if (!info->mute_is_active) {
    info->seq_num_offset = info->last_original_seq_num - info->last_sent_seq_num;
    ELOG_DEBUG("%s message: Deactivated, original_seq_num: %u, last_sent_seq_num: %u, offset: %u",
        connection_->toLog(), info->last_original_seq_num, info->last_sent_seq_num, info->seq_num_offset);
  } else {
    if (info->label == "video") {
      getContext()->fireRead(RtpUtils::createPLI(connection_->getVideoSinkSSRC(), connection_->getVideoSourceSSRC()));
    }
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

}  // namespace erizo
