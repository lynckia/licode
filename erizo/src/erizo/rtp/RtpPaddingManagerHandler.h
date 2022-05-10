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

enum PaddingManagerMode {
  START = 0,
  STABLE = 1,
  HOLD = 2,
  RECOVER = 3
};


class RtpPaddingManagerHandler: public Handler, public std::enable_shared_from_this<RtpPaddingManagerHandler> {
  DECLARE_LOGGER();

 public:
  static constexpr duration kMaxDurationInStartMode = std::chrono::seconds(15);
  static constexpr duration kMaxDurationInRecoverMode = std::chrono::seconds(10);
  static constexpr duration kMaxDurationInHoldMode = std::chrono::seconds(5);
  static constexpr double kBweSharpDropThreshold = 0.66;
  static constexpr double kStartModeFactor = 3;
  static constexpr double kRecoverBweFactor = 0.85;
  static constexpr double kStableModeAvailableFactor = 1.1;
  explicit RtpPaddingManagerHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<erizo::SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "padding-calculator";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;
  PaddingManagerMode getCurrentPaddingMode();

 private:
  bool isTimeToCalculateBitrate();
  void recalculatePaddingRate();
  void distributeTotalTargetPaddingBitrate(int64_t bitrate);
  int64_t getTotalTargetBitrate();
  void maybeTriggerTimedModeChanges();
  void forceModeSwitch(PaddingManagerMode new_mode);

 private:
  bool initialized_;
  std::shared_ptr<erizo::Clock> clock_;
  time_point last_rate_calculation_time_;
  time_point last_time_bwe_decreased_;
  time_point last_mode_change_;
  uint64_t estimated_before_drop_;
  WebRtcConnection* connection_;
  std::shared_ptr<Stats> stats_;
  int64_t last_estimated_bandwidth_;
  bool can_recover_;
  PaddingManagerMode current_mode_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPPADDINGMANAGERHANDLER_H_
