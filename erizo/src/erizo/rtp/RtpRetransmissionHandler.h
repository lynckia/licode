#ifndef ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_

#include "pipeline/Handler.h"

#include "./WebRtcConnection.h"

static constexpr uint kRetransmissionsBufferSize = 257;
static constexpr int kNackBlpSize = 16;

namespace erizo {
class RtpRetransmissionHandler : public Handler {
 public:
  DECLARE_LOGGER();

  explicit RtpRetransmissionHandler(WebRtcConnection *connection)
    : connection_{connection},
    audio_{kRetransmissionsBufferSize},
    video_{kRetransmissionsBufferSize} {}

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override {
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

  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override {
    // ELOG_DEBUG("%p WRITING %d bytes", this, packet->length);
    RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
    if (!chead->isRtcp()) {
      if (connection_->getVideoSinkSSRC() == head->getSSRC()) {
        video_[getIndexInBuffer(head->getSeqNumber())] = packet;
      } else if (connection_->getAudioSinkSSRC() == head->getSSRC()) {
        audio_[getIndexInBuffer(head->getSeqNumber())] = packet;
      }
    }
    ctx->fireWrite(packet);
  }

 private:
  uint16_t getIndexInBuffer(uint16_t seq_num) {
    return seq_num % kRetransmissionsBufferSize;
  }

 private:
  WebRtcConnection *connection_;
  std::vector<std::shared_ptr<dataPacket>> audio_;
  std::vector<std::shared_ptr<dataPacket>> video_;
};

DEFINE_LOGGER(RtpRetransmissionHandler, "rtp.RtpRetransmissionHandler");
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
