#ifndef ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_


#include "./logger.h"
#include "pipeline/Handler.h"
#include "./Stats.h"
#include "rtp/QualityManager.h"

namespace erizo {


constexpr duration kLayerRateStatIntervalSize = std::chrono::milliseconds(100);
constexpr uint32_t kLayerRateStatIntervals = 30;

class LayerBitrateCalculationHandler: public OutboundHandler {
  DECLARE_LOGGER();


 public:
  LayerBitrateCalculationHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "layer_bitrate_calculator";
  }

  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  const std::string kQualityLayersStatsKey = "qualityLayers";
  bool enabled_;
  bool initialized_;
  std::shared_ptr<Stats> stats_;
  std::shared_ptr<QualityManager> quality_manager_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_LAYERBITRATECALCULATIONHANDLER_H_
