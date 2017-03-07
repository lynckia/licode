#ifndef ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_


#include "./logger.h"
#include "pipeline/Handler.h"
#include "./Stats.h"

namespace erizo {

class WebRtcConnection;

constexpr duration kLayerRateStatIntervalSize = std::chrono::milliseconds(100);
constexpr uint32_t kLayerRateStatIntervals = 10;

class LayerBitrateCalculationHandler: public OutboundHandler {
  DECLARE_LOGGER();


 public:
  LayerBitrateCalculationHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "layer_bitrate_calculator";
  }

  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  bool enabled_;
  bool initialized_;
  std::shared_ptr<Stats> stats_;
  const std::string kQualityLayersStatsKey = "qualityLayers";
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_

