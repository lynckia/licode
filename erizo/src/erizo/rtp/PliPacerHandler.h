#ifndef ERIZO_SRC_ERIZO_RTP_PLIPACERHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_PLIPACERHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "thread/Worker.h"
#include "lib/Clock.h"

namespace erizo {

class MediaStream;

class PliPacerHandler: public Handler, public std::enable_shared_from_this<PliPacerHandler> {
  DECLARE_LOGGER();

 public:
  static constexpr duration kMinPLIPeriod = std::chrono::milliseconds(200);
  static constexpr duration kKeyframeTimeout = std::chrono::seconds(10);

 public:
  explicit PliPacerHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "pli-pacer";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void scheduleNextPLI();
  void sendPLI();
  void sendFIR();

 private:
  bool enabled_;
  MediaStream* stream_;
  std::shared_ptr<erizo::Clock> clock_;
  time_point time_last_keyframe_;
  bool waiting_for_keyframe_;
  std::shared_ptr<ScheduledTaskReference> scheduled_pli_;
  uint32_t video_sink_ssrc_;
  uint32_t video_source_ssrc_;
  uint8_t fir_seq_number_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_PLIPACERHANDLER_H_
