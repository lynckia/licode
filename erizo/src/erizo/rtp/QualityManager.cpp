#include "rtp/QualityManager.h"

namespace erizo {

DEFINE_LOGGER(QualityManager, "rtp.QualityManager");

constexpr duration QualityManager::kMinLayerChangeInterval;
constexpr duration QualityManager::kActiveLayerInterval;

QualityManager::QualityManager()
  : initialized_{false}, spatial_layer_{0}, temporal_layer_{0}, current_estimated_bitrate_{0},
  last_quality_check_{clock::now()} {}


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
  time_point now = clock::now();
  uint64_t estimated_bitrate = stats_->getNode()["total"]["senderBitrateEstimation"].value();
  bool available_bitrate_is_descending = estimated_bitrate < current_estimated_bitrate_;
  current_estimated_bitrate_ = estimated_bitrate;
  if (!isInBaseLayer() &&  (available_bitrate_is_descending || !isCurrentLayerPresent())) {
    ELOG_DEBUG("message: Forcing calculate new layer, "
        "available_bitrate_is_descending: %d")
    selectLayer();
    return;
  }
  if (now - last_quality_check_ > kMinLayerChangeInterval) {
    selectLayer();
  }
}

void QualityManager::selectLayer() {
  last_quality_check_ = clock::now();
  int aux_temporal_layer = 0;
  int aux_spatial_layer = 0;
  int next_temporal_layer = temporal_layer_;
  int next_spatial_layer = spatial_layer_;
  ELOG_DEBUG("Calculate best layer with %lu", current_estimated_bitrate_);
  for (auto &spatial_layer_node : stats_->getNode()["qualityLayers"].getMap()) {
    for (auto temporal_layer_node : stats_->getNode()["qualityLayers"][spatial_layer_node.first.c_str()].getMap()) {
     // ELOG_DEBUG("Bitrate for layer %d/%d %lu",
         // aux_spatial_layer, aux_temporal_layer, temporal_layer_node.second->value());
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
    ELOG_DEBUG("message: Changing Layer, current_layers: %d/%d, new_layers: %d/%d",
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
}  // namespace erizo
