#ifndef ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_

#include <memory>
#include <string>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/RtcpRrGenerator.h"
#include "rtp/RtcpNewNackGenerator.h"
#include "lib/ClockUtils.h"

#define MAX_DELAY 450000

namespace erizo {

class MediaStream;

class RtcpGeneratorPair {
 public:
  std::shared_ptr<RtcpRrGenerator> rr_generator;
  std::shared_ptr<RtcpNewNackGenerator> nack_generator;
};


class RtcpFeedbackGenerationHandler: public Handler {
  DECLARE_LOGGER();


 public:
  explicit RtcpFeedbackGenerationHandler(bool nacks_enabled = true, bool pli_enabled = true,
      std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());


  void enable() override;
  void disable() override;

  std::string getName() override {
     return "rtcp_feedback_generation";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  MediaStream *stream_;
  std::map<uint32_t, std::shared_ptr<RtcpGeneratorPair>> generators_map_;

  bool enabled_, initialized_;
  bool nacks_enabled_;
  bool pli_enabled_;
  std::shared_ptr<Clock> clock_;

  uint32_t video_sink_ssrc_ = 0;
  uint32_t audio_sink_ssrc_ = 0;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_
