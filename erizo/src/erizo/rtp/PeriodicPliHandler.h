#ifndef ERIZO_SRC_ERIZO_RTP_PERIODICPLIHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_PERIODICPLIHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "thread/Worker.h"
#include "lib/Clock.h"

namespace erizo {

class MediaStream;

class PeriodicPliHandler: public Handler, public std::enable_shared_from_this<PeriodicPliHandler> {
  DECLARE_LOGGER();

 public:
  explicit PeriodicPliHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "periodic-pli";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;
  void updateInterval(bool active, uint32_t interval_ms);

 private:
  void scheduleNextPli(duration next_pli_time);
  void sendPLI();

 private:
  bool enabled_;
  MediaStream* stream_;
  std::shared_ptr<erizo::Clock> clock_;
  uint32_t video_sink_ssrc_;
  uint32_t video_source_ssrc_;
  uint32_t keyframes_received_in_interval_;
  duration requested_interval_;
  bool requested_periodic_plis_;

  bool has_scheduled_pli_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_PERIODICPLIHANDLER_H_

