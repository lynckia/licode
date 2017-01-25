#ifndef ERIZO_SRC_ERIZO_RTP_STATSHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_STATSHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"

namespace erizo {

class WebRtcConnection;

class IncomingStatsHandler: public InboundHandler {
  DECLARE_LOGGER();

 public:
  explicit IncomingStatsHandler(WebRtcConnection* connection);

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "incoming-stats";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;

 private:
  WebRtcConnection* connection_;
};

class OutgoingStatsHandler: public OutboundHandler {
  DECLARE_LOGGER();

 public:
  explicit OutgoingStatsHandler(WebRtcConnection* connection);

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "outgoing-stats";
  }

  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;

 private:
  WebRtcConnection* connection_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_STATSHANDLER_H_
