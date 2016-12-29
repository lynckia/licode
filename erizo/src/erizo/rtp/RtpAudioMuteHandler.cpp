#include "rtp/RtpAudioMuteHandler.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

namespace erizo {

DEFINE_LOGGER(RtpAudioMuteHandler, "rtp.RtpAudioMuteHandler");

RtpAudioMuteHandler::RtpAudioMuteHandler(WebRtcConnection *connection) :
  last_original_seq_num_{-1}, seq_num_offset_{0}, mute_is_active_{false}, connection_{connection} {}

void RtpAudioMuteHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (connection_->getAudioSinkSSRC() != chead->getSourceSSRC()) {
    ctx->fireRead(packet);
    return;
  }
  control_mutex_.lock();
  if (seq_num_offset_ > 0) {
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
          if ((chead->getHighestSeqnum() + seq_num_offset_) < chead->getHighestSeqnum()) {
            // The seqNo adjustment causes a wraparound, add to cycles
            chead->setSeqnumCycles(chead->getSeqnumCycles() + 1);
          }
          chead->setHighestSeqnum(chead->getHighestSeqnum() + seq_num_offset_);

          break;
        case RTCP_RTP_Feedback_PT:
          chead->setNackPid(chead->getNackPid() + seq_num_offset_);
          break;
        default:
          break;
      }
    } while (total_length < packet->length);
  }
  control_mutex_.unlock();
  ctx->fireRead(packet);
}

void RtpAudioMuteHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);
  if (packet->type != AUDIO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireWrite(packet);
    return;
  }
  control_mutex_.lock();
  last_original_seq_num_ = rtp_header->getSeqNumber();
  if (mute_is_active_) {
    control_mutex_.unlock();
  } else {
    last_sent_seq_num_ = last_original_seq_num_ - seq_num_offset_;
    if (seq_num_offset_ > 0) {
      setPacketSeqNumber(packet, last_sent_seq_num_);
    }
    control_mutex_.unlock();
    ctx->fireWrite(packet);
  }
}

void RtpAudioMuteHandler::muteAudio(bool active) {
  boost::mutex::scoped_lock lock(control_mutex_);
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
