#ifndef ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_


#include "./logger.h"
#include "pipeline/Handler.h"

namespace erizo {

class WebRtcConnection;


class LayerBitrateCalculationHandler: public InboundHandler {
  DECLARE_LOGGER();


 public:
  LayerBitrateCalculationHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "layer_bitrate_calculator";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  WebRtcConnection *connection_;
  bool enabled_;
  bool initialized_;
  std::vector<uint32_t> video_ssrc_list_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_

