#ifndef ERIZO_SRC_ERIZO_RTP_TEMPORALLAYERSWITCHHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_TEMPORALLAYERSWITCHHANDLER_H_

#include <string>

#include "pipeline/Handler.h"
#include "./logger.h"
#include "./WebRtcConnection.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtpVP9Parser.h"

namespace erizo {
class TemporalLayerSwitchHandler : public Handler {
  DECLARE_LOGGER();

 public:
  TemporalLayerSwitchHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "temp-layer";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  bool isFrameInLayer(std::shared_ptr<dataPacket> packet, int temporal_layer, unsigned int ssrc);

 private:
  WebRtcConnection* connection_;
  RtpVP8Parser vp8_parser_;
  int32_t last_sent_seq_num_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_TEMPORALLAYERSWITCHHANDLER_H_
