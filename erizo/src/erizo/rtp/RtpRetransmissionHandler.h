#ifndef ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_

#include "pipeline/Handler.h"

namespace erizo {

class RtpRetransmissionHandler : public Handler {
 public:
  DECLARE_LOGGER();

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override {
    // ELOG_DEBUG("READING %d bytes", packet->length);
    ctx->fireRead(packet);
  }
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override {
    // ELOG_DEBUG("WRITING %d bytes", packet->length);
    ctx->fireWrite(packet);
  }
};

DEFINE_LOGGER(RtpRetransmissionHandler, "rtp.RtpRetransmissionHandler");
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
