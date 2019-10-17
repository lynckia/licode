#ifndef ERIZO_SRC_ERIZO_RTP_PLIPRIORITYHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_PLIPRIORITYHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "thread/Worker.h"
#include "lib/Clock.h"

namespace erizo {

class MediaStream;

class PliPriorityHandler: public Handler, public std::enable_shared_from_this<PliPriorityHandler> {
  DECLARE_LOGGER();

 public:
  static constexpr duration kLowPriorityPliPeriod = std::chrono::seconds(3);

 public:
  explicit PliPriorityHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "pli-priority";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void schedulePeriodicPlis();
  void sendPLI();

 private:
  bool enabled_;
  MediaStream* stream_;
  std::shared_ptr<erizo::Clock> clock_;
  uint32_t video_sink_ssrc_;
  uint32_t video_source_ssrc_;
  uint32_t plis_received_in_interval_;
  bool first_received_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_PLIPRIORITYHANDLER_H_

