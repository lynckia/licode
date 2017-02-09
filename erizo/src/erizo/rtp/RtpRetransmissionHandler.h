#ifndef ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_

#include <vector>

#include "pipeline/Handler.h"

#include "./WebRtcConnection.h"

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
  uint16_t getIndexInBuffer(uint16_t seq_num);

 private:
  WebRtcConnection *connection_;
  std::vector<std::shared_ptr<dataPacket>> audio_;
  std::vector<std::shared_ptr<dataPacket>> video_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
