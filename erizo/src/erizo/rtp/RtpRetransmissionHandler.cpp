#include "rtp/RtpRetransmissionHandler.h"

namespace erizo {

DEFINE_LOGGER(RtpRetransmissionHandler, "rtp.RtpRetransmissionHandler");

RtpRetransmissionHandler::RtpRetransmissionHandler(WebRtcConnection *connection)
  : connection_{connection},
  audio_{kRetransmissionsBufferSize},
  video_{kRetransmissionsBufferSize},
  buffer_mutex_ptr_{std::make_shared<boost::mutex>()} {}

void RtpRetransmissionHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  // ELOG_DEBUG("%p READING %d bytes", this, packet->length);
  bool contains_nack = false;
  bool is_fully_recovered = true;
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp() && chead->isFeedback()) {
    char* movingBuf = packet->data;
    int rtcpLength = 0;
    int totalLength = 0;

    do {
      movingBuf += rtcpLength;
      chead = reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength = (ntohs(chead->length) + 1) * 4;
      totalLength += rtcpLength;

      if (chead->packettype == RTCP_RTP_Feedback_PT) {
        contains_nack = true;
        uint16_t initial_seq_num = chead->getNackPid();
        uint16_t plb = chead->getNackBlp();

        for (int i = -1; i <= kNackBlpSize; i++) {
          uint16_t seq_num = initial_seq_num + i + 1;
          bool packet_nacked = i == -1 || (plb >> i) & 0x0001;

          if (packet_nacked) {
            boost::mutex::scoped_lock lock(*buffer_mutex_ptr_.get());
            std::shared_ptr<dataPacket> recovered;

            if (connection_->getVideoSinkSSRC() == chead->getSourceSSRC()) {
              recovered = video_[getIndexInBuffer(seq_num)];
            } else if (connection_->getAudioSinkSSRC() == chead->getSourceSSRC()) {
              recovered = audio_[getIndexInBuffer(seq_num)];
            }

            if (recovered.get()) {
              RtpHeader *recovered_head = reinterpret_cast<RtpHeader*> (recovered->data);
              if (recovered_head->getSeqNumber() == seq_num) {
                ctx->fireWrite(recovered);
                continue;
              }
            }
            ELOG_DEBUG("Packet missed in buffer %d", seq_num);
            is_fully_recovered = false;
            break;
          }
        }
      }
    } while (totalLength < packet->length);
  }
  if (!contains_nack || !is_fully_recovered) {
    ctx->fireRead(packet);
  }
}

void RtpRetransmissionHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  // ELOG_DEBUG("%p WRITING %d bytes", this, packet->length);
  RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (!chead->isRtcp()) {
    boost::mutex::scoped_lock lock(*buffer_mutex_ptr_.get());
    if (connection_->getVideoSinkSSRC() == head->getSSRC()) {
      video_[getIndexInBuffer(head->getSeqNumber())] = packet;
    } else if (connection_->getAudioSinkSSRC() == head->getSSRC()) {
      audio_[getIndexInBuffer(head->getSeqNumber())] = packet;
    }
  }
  ctx->fireWrite(packet);
}

uint16_t RtpRetransmissionHandler::getIndexInBuffer(uint16_t seq_num) {
  return seq_num % kRetransmissionsBufferSize;
}
}  // namespace erizo
