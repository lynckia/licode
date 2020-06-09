#ifndef ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_

#include <memory>
#include <string>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/RtcpRrGenerator.h"
#include "rtp/RtcpNackGenerator.h"
#include "lib/ClockUtils.h"
#include "./MediaDefinitions.h"

#define MAX_DELAY 450000

namespace erizo {

class MediaStream;

class PublisherRtpInfoEvent : public MediaEvent {
 public:
  explicit PublisherRtpInfoEvent(std::string kind, uint8_t fraction_lost)
    : kind_{kind}, fraction_lost_{fraction_lost} {}

  std::string getType() const override {
    return "PublisherRtpInfoEvent";
  }

  std::string kind_;
  uint8_t fraction_lost_;
};

class RtcpGeneratorPair {
 public:
  std::shared_ptr<RtcpRrGenerator> rr_generator;
  std::shared_ptr<RtcpNackGenerator> nack_generator;
};


class RtcpFeedbackGenerationHandler: public Handler {
  DECLARE_LOGGER();


 public:
  explicit RtcpFeedbackGenerationHandler(bool nacks_enabled = true,
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
  void notifyReceiverReportInfo(const std::shared_ptr<DataPacket> &packet, bool is_audio);

 private:
  MediaStream *stream_;
  std::map<uint32_t, std::shared_ptr<RtcpGeneratorPair>> generators_map_;

  bool enabled_, initialized_;
  bool nacks_enabled_;
  std::shared_ptr<Clock> clock_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_
