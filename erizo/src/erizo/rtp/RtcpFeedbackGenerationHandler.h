#ifndef ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_

#include <memory>
#include <string>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/RtcpRrGenerator.h"
#include "rtp/RtcpNackGenerator.h"

#define MAX_DELAY 450000

namespace erizo {

class WebRtcConnection;

class RtcpGeneratorPair {
 public:
  std::shared_ptr<RtcpRrGenerator> rr_generator;
  std::shared_ptr<RtcpNackGenerator> nack_generator;
};


class RtcpFeedbackGenerationHandler: public Handler {
  DECLARE_LOGGER();


 public:
  RtcpFeedbackGenerationHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "rtcp_feedback_generation";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  WebRtcConnection *connection_;
  std::map<uint32_t, std::shared_ptr<RtcpGeneratorPair>> generators_map_;

  bool enabled_, initialized_;
  bool nacks_enabled_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPFEEDBACKGENERATIONHANDLER_H_
