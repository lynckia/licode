#ifndef ERIZO_SRC_ERIZO_RTP_RTCPPROCESSORHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPPROCESSORHANDLER_H_

#include <string>

#include "./logger.h"
#include "./Stats.h"
#include "pipeline/Handler.h"
#include "rtp/RtcpProcessor.h"

namespace erizo {

class MediaStream;

class RtcpProcessorHandler: public Handler {
  DECLARE_LOGGER();

 public:
  RtcpProcessorHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "rtcp-processor";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  MediaStream* stream_;
  std::shared_ptr<RtcpProcessor> processor_;
  std::shared_ptr<Stats> stats_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPPROCESSORHANDLER_H_
