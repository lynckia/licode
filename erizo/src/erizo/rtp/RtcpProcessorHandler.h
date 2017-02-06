#ifndef ERIZO_SRC_ERIZO_RTP_RTCPPROCESSORHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPPROCESSORHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/RtcpProcessor.h"

namespace erizo {

class WebRtcConnection;

class RtcpProcessorHandler: public Handler {
  DECLARE_LOGGER();

 public:
  explicit RtcpProcessorHandler(WebRtcConnection* connection, std::shared_ptr<RtcpProcessor> processor);

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "rtcp-processor";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  WebRtcConnection* connection_;
  std::shared_ptr<RtcpProcessor> processor_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPPROCESSORHANDLER_H_
