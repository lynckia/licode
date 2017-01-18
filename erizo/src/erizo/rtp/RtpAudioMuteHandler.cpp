#include "rtp/RtpAudioMuteHandler.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

namespace erizo {

DEFINE_LOGGER(RtpAudioMuteHandler, "rtp.RtpAudioMuteHandler");

RtpAudioMuteHandler::RtpAudioMuteHandler(WebRtcConnection *connection) :
  last_original_seq_num_{-1}, seq_num_offset_{0}, mute_is_active_{false}, connection_{connection} {}


void RtpAudioMuteHandler::enable() {
}

void RtpAudioMuteHandler::disable() {
}

void RtpAudioMuteHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (connection_->getAudioSinkSSRC() != chead->getSourceSSRC()) {
    ctx->fireRead(packet);
    return;
  }
  uint16_t offset;
  {
    std::lock_guard<std::mutex> lock(control_mutex_);
    offset = seq_num_offset_;
  }
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
  ctx->fireRead(packet);
}

void RtpAudioMuteHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);
  if (packet->type != AUDIO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireWrite(packet);
    return;
  }
  bool is_muted;
  uint16_t offset;
  {
    std::lock_guard<std::mutex> lock(control_mutex_);
    is_muted = mute_is_active_;
    offset = seq_num_offset_;
  }
  last_original_seq_num_ = rtp_header->getSeqNumber();
  if (!is_muted) {
    last_sent_seq_num_ = last_original_seq_num_ - offset;
    if (offset > 0) {
      setPacketSeqNumber(packet, last_sent_seq_num_);
    }
    ctx->fireWrite(packet);
  }
}

void RtpAudioMuteHandler::muteAudio(bool active) {
  std::lock_guard<std::mutex> lock(control_mutex_);
  mute_is_active_ = active;
  ELOG_INFO("%s message: Mute Audio, active: %d", connection_->toLog(), active);
  if (!mute_is_active_) {
    seq_num_offset_ = last_original_seq_num_ - last_sent_seq_num_;
    ELOG_DEBUG("%s message: Deactivated, original_seq_num: %u, last_sent_seq_num: %u, offset: %u",
        connection_->toLog(), last_original_seq_num_, last_sent_seq_num_, seq_num_offset_);
  }
}

inline void RtpAudioMuteHandler::setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number) {
  RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    return;
  }
  head->setSeqNumber(seq_number);
}

}  // namespace erizo
