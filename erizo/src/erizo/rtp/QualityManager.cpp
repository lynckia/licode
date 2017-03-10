#include "rtp/QualityManager.h"

namespace erizo {

DEFINE_LOGGER(QualityManager, "rtp.QualityManager");

constexpr duration QualityManager::kMinLayerSwitchInterval;
constexpr duration QualityManager::kActiveLayerInterval;

QualityManager::QualityManager(std::shared_ptr<Clock> the_clock)
  : initialized_{false}, padding_enabled_{false}, forced_layers_{false},
  spatial_layer_{0}, temporal_layer_{0},
  current_estimated_bitrate_{0}, last_quality_check_{the_clock->now()},
  clock_{the_clock} {}


void QualityManager::notifyQualityUpdate() {
  if (!initialized_) {
    auto pipeline = getContext()->getPipelineShared();
    if (!pipeline) {
      return;
    }
    stats_ = pipeline->getService<Stats>();
    if (!stats_) {
      return;
    }
    if (!stats_->getNode()["total"].hasChild("senderBitrateEstimation")) {
      return;
    }
    initialized_ = true;
  }
  if (forced_layers_) {
    return;
  }
  time_point now = clock_->now();
  uint64_t estimated_bitrate = stats_->getNode()["total"]["senderBitrateEstimation"].value();
  bool available_bitrate_is_descending = estimated_bitrate < current_estimated_bitrate_;
  current_estimated_bitrate_ = estimated_bitrate;
  if (!isInBaseLayer() &&  (available_bitrate_is_descending || !isCurrentLayerPresent())) {
    ELOG_DEBUG("message: Forcing calculate new layer, "
        "available_bitrate_is_descending: %d, isInBaseLayer: %d", available_bitrate_is_descending, isInBaseLayer());
    selectLayer();
    return;
  } else if (now - last_quality_check_ > kMinLayerSwitchInterval) {
    selectLayer();
  }
}

void QualityManager::selectLayer() {
  last_quality_check_ = clock_->now();
  int aux_temporal_layer = 0;
  int aux_spatial_layer = 0;
  int next_temporal_layer = 0;
  int next_spatial_layer = 0;
  ELOG_DEBUG("Calculate best layer with %lu, current layer %d/%d",
      current_estimated_bitrate_, spatial_layer_, temporal_layer_);
  for (auto &spatial_layer_node : stats_->getNode()["qualityLayers"].getMap()) {
    for (auto temporal_layer_node : stats_->getNode()["qualityLayers"][spatial_layer_node.first.c_str()].getMap()) {
     ELOG_DEBUG("Bitrate for layer %d/%d %lu",
         aux_spatial_layer, aux_temporal_layer, temporal_layer_node.second->value());
      if (temporal_layer_node.second->value() != 0 &&
          temporal_layer_node.second->value() < current_estimated_bitrate_) {
        next_temporal_layer = aux_temporal_layer;
        next_spatial_layer = aux_spatial_layer;
      }
      aux_temporal_layer++;
    }
    aux_temporal_layer = 0;
    aux_spatial_layer++;
  }
  if (next_temporal_layer != temporal_layer_ || next_spatial_layer != spatial_layer_) {
    ELOG_DEBUG("message: Layer Switch, current_layer: %d/%d, new_layer: %d/%d",
        spatial_layer_, temporal_layer_, next_spatial_layer, next_temporal_layer);
    setTemporalLayer(next_temporal_layer);
    setSpatialLayer(next_spatial_layer);
  }
}

bool QualityManager::isCurrentLayerPresent() {
  if (isInBaseLayer()) {
    return true;
  }
  MovingIntervalRateStat* current_layer_stat =
    reinterpret_cast<MovingIntervalRateStat*>(&stats_->getNode()["qualityLayers"][spatial_layer_][temporal_layer_]);
  uint64_t selected_layer_bitrate = current_layer_stat->value(kActiveLayerInterval);
  bool isLayerPresent = selected_layer_bitrate != 0;
  if (!isLayerPresent) {
    ELOG_DEBUG("message: Layer is no longer present, spatial_layer: %u, temporal_layer %u",
        spatial_layer_, temporal_layer_);
  }
  return isLayerPresent;
}

bool QualityManager::isInBaseLayer() {
  return (spatial_layer_ == 0 && temporal_layer_ == 0);
}

void QualityManager::forceLayers(int spatial_layer, int temporal_layer) {
  forced_layers_ = true;
  spatial_layer_ = spatial_layer;
  temporal_layer_ = temporal_layer;
}

void QualityManager::setSpatialLayer(int spatial_layer) {
  if (!forced_layers_) {
    spatial_layer_ = spatial_layer;
  }
}
void QualityManager::setTemporalLayer(int temporal_layer) {
  if (!forced_layers_) {
    temporal_layer_ = temporal_layer;
  }
}

}  // namespace erizo
