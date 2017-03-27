#ifndef ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_

#include <memory>
#include <vector>

#include "pipeline/Handler.h"
#include "lib/Clock.h"
#include "lib/TokenBucket.h"

#include "./WebRtcConnection.h"

static constexpr uint kRetransmissionsBufferSize = 256;
static constexpr int kNackBlpSize = 16;

static constexpr erizo::duration kTimeToUpdateBitrate = std::chrono::milliseconds(500);
static constexpr float kMarginRtxBitrate = 0.1;
static constexpr int kBurstSize = 1300 * 20;  // 20 packets with almost max size

namespace erizo {
class RtpRetransmissionHandler : public Handler {
 public:
  DECLARE_LOGGER();

  explicit RtpRetransmissionHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<erizo::SteadyClock>());

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
  MovingIntervalRateStat& getRtxBitrateStat();
  uint64_t getBitrateCalculated();
  void calculateRtxBitrate();

 private:
  std::shared_ptr<erizo::Clock> clock_;
  WebRtcConnection *connection_;
  std::vector<std::shared_ptr<dataPacket>> audio_;
  std::vector<std::shared_ptr<dataPacket>> video_;
  std::shared_ptr<Stats> stats_;
  TokenBucket bucket_;
  erizo::time_point last_bitrate_time_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
