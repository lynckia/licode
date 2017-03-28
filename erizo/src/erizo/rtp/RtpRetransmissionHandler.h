#ifndef ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_

#include <vector>

#include "pipeline/Handler.h"

#include "./WebRtcConnection.h"
#include "rtp/PacketBufferService.h"

static constexpr uint kRetransmissionsBufferSize = 256;
static constexpr int kNackBlpSize = 16;

namespace erizo {
class RtpRetransmissionHandler : public Handler {
 public:
  DECLARE_LOGGER();

  RtpRetransmissionHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "retransmissions";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  WebRtcConnection *connection_;
  bool initialized_, enabled_;
  std::vector<std::shared_ptr<dataPacket>> audio_;
  std::vector<std::shared_ptr<dataPacket>> video_;
  std::shared_ptr<Stats> stats_;
  std::shared_ptr<PacketBufferService> packet_buffer_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
