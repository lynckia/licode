#ifndef ERIZO_SRC_ERIZO_RTP_LAYERDETECTORHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_LAYERDETECTORHANDLER_H_

#include <memory>
#include <string>
#include <random>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtpVP9Parser.h"

#define MAX_DELAY 450000

namespace erizo {

class WebRtcConnection;


class LayerDetectorHandler: public InboundHandler, public std::enable_shared_from_this<LayerDetectorHandler> {
  DECLARE_LOGGER();


 public:
  LayerDetectorHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "layer_detector";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void parseLayerInfoFromVP8(std::shared_ptr<dataPacket> packet);
  void parseLayerInfoFromVP9(std::shared_ptr<dataPacket> packet);
  int getSsrcPosition(uint32_t ssrc);

 private:
  WebRtcConnection *connection_;
  bool enabled_;
  bool initialized_;
  RtpVP8Parser vp8_parser_;
  RtpVP9Parser vp9_parser_;
  std::vector<uint32_t> video_ssrc_list_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_LAYERDETECTORHANDLER_H_
