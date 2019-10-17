#ifndef ERIZO_SRC_ERIZO_RTP_RTPPADDINGMANAGERHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPPADDINGMANAGERHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "lib/Clock.h"
#include "lib/TokenBucket.h"
#include "thread/Worker.h"
#include "rtp/SequenceNumberTranslator.h"
#include "./Stats.h"

namespace erizo {

class WebRtcConnection;

class RtpPaddingManagerHandler: public Handler, public std::enable_shared_from_this<RtpPaddingManagerHandler> {
  DECLARE_LOGGER();

 public:
  explicit RtpPaddingManagerHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<erizo::SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "padding-calculator";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  bool isTimeToCalculateBitrate();
  void recalculatePaddingRate();
  void distributeTotalTargetPaddingBitrate(int64_t bitrate);
  int64_t getTotalTargetBitrate();

 private:
  bool initialized_;
  std::shared_ptr<erizo::Clock> clock_;
  time_point last_rate_calculation_time_;
  time_point last_time_with_packet_losses_;
  WebRtcConnection* connection_;
  std::shared_ptr<Stats> stats_;
  int64_t last_estimated_bandwidth_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPPADDINGMANAGERHANDLER_H_
