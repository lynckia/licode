#include "rtp/LayerBitrateCalculationHandler.h"

#include <vector>

#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(LayerBitrateCalculationHandler, "rtp.LayerBitrateCalculationHandler");

LayerBitrateCalculationHandler::LayerBitrateCalculationHandler() : enabled_{true},
  initialized_{false} {}

void LayerBitrateCalculationHandler::enable() {
  enabled_ = true;
}

void LayerBitrateCalculationHandler::disable() {
  enabled_ = false;
}

void LayerBitrateCalculationHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  if (!enabled_ || !initialized_) {
    ctx->fireWrite(std::move(packet));
    return;
  }

  std::for_each(packet->compatible_spatial_layers.begin(),
      packet->compatible_spatial_layers.end(), [this, packet](int &layer_num){
        std::string spatial_layer_name = std::to_string(layer_num);
        std::for_each(packet->compatible_temporal_layers.begin(),
          packet->compatible_temporal_layers.end(), [this, packet, spatial_layer_name](int &layer_num){
            std::string temporal_layer_name = std::to_string(layer_num);
            if (!stats_->getNode()[kQualityLayersStatsKey][spatial_layer_name].hasChild(temporal_layer_name)) {
              stats_->getNode()[kQualityLayersStatsKey][spatial_layer_name].insertStat(
                  temporal_layer_name, MovingIntervalRateStat{kLayerRateStatIntervalSize,
                  kLayerRateStatIntervals, 8.});
            } else {
              stats_->getNode()[kQualityLayersStatsKey][spatial_layer_name][temporal_layer_name]+=packet->length;
            }
          });
      });
  quality_manager_->notifyQualityUpdate();
  ctx->fireWrite(std::move(packet));
}


void LayerBitrateCalculationHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }
  stats_ = pipeline->getService<Stats>();
  if (!stats_) {
    return;
  }
  quality_manager_ = pipeline->getService<QualityManager>();
  if (!quality_manager_) {
    return;
  }
  initialized_ = true;
}
}  // namespace erizo
