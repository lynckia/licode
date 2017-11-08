#include "rtp/QualityManager.h"
#include <memory>

#include "WebRtcConnection.h"
#include "rtp/LayerDetectorHandler.h"

namespace erizo {

DEFINE_LOGGER(QualityManager, "rtp.QualityManager");

constexpr duration QualityManager::kMinLayerSwitchInterval;
constexpr duration QualityManager::kActiveLayerInterval;
constexpr float QualityManager::kIncreaseLayerBitrateThreshold;

QualityManager::QualityManager(std::shared_ptr<Clock> the_clock)
  : initialized_{false}, enabled_{false}, padding_enabled_{false}, forced_layers_{false},
  slideshow_mode_active_{false}, spatial_layer_{0}, temporal_layer_{0}, max_active_spatial_layer_{0},
  max_active_temporal_layer_{0}, max_video_width_{-1}, max_video_height_{-1},
  max_video_frame_rate_{-1}, current_estimated_bitrate_{0}, last_quality_check_{the_clock->now()},
  last_activity_check_{the_clock->now()}, clock_{the_clock} {}

void QualityManager::enable() {
  ELOG_DEBUG("message: Enabling QualityManager");
  enabled_ = true;
  if (!forced_layers_) {
    setPadding(true);
  }
}

void QualityManager::disable() {
  ELOG_DEBUG("message: Disabling QualityManager");
  enabled_ = false;
  setPadding(false);
}

void QualityManager::notifyEvent(MediaEventPtr event) {
  if (event->getType() == "LayerInfoChangedEvent") {
    auto layer_event = std::static_pointer_cast<LayerInfoChangedEvent>(event);
    video_frame_width_list_ = layer_event->video_frame_width_list;
    video_frame_height_list_ = layer_event->video_frame_height_list;
    video_frame_rate_list_ = layer_event->video_frame_rate_list;
  }
}

void QualityManager::notifyQualityUpdate() {
  if (!enabled_) {
    return;
  }

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
  current_estimated_bitrate_ = stats_->getNode()["total"]["senderBitrateEstimation"].value();
  uint64_t current_layer_instant_bitrate = getInstantLayerBitrate(spatial_layer_, temporal_layer_);
  bool estimated_is_under_layer_bitrate = current_estimated_bitrate_ < current_layer_instant_bitrate;

  if (now - last_activity_check_ > kActiveLayerInterval) {
    calculateMaxActiveLayer();
    last_activity_check_ = now;
  }

  bool layer_is_active = spatial_layer_ <= max_active_spatial_layer_;

  if (!layer_is_active || (estimated_is_under_layer_bitrate && !slideshow_mode_active_)) {
    ELOG_DEBUG("message: Forcing calculate new layer, "
        "estimated_is_under_layer_bitrate: %d, layer_is_active: %d", estimated_is_under_layer_bitrate,
        layer_is_active);
    selectLayer(false);
  } else if (now - last_quality_check_ > kMinLayerSwitchInterval) {
    selectLayer(true);
  }
}

bool QualityManager::doesLayerMeetConstraints(int spatial_layer, int temporal_layer) {
  if (static_cast<uint>(spatial_layer) > video_frame_width_list_.size() ||
      static_cast<uint>(spatial_layer) > video_frame_height_list_.size() ||
      static_cast<uint>(temporal_layer) > video_frame_rate_list_.size()) {
        return true;
  }
  if (spatial_layer == 0 && temporal_layer == 0) {
    return true;
  }
  int64_t layer_width = video_frame_width_list_[spatial_layer];
  int64_t layer_height = video_frame_height_list_[spatial_layer];
  int64_t layer_frame_rate = video_frame_rate_list_[temporal_layer];

  if (layer_width == 0 || layer_height == 0 || layer_frame_rate == 0) {
    return true;
  }

  bool max_resolution_not_set = max_video_width_ == -1 && max_video_height_ == -1;
  bool max_frame_rate_not_set = max_video_frame_rate_ == -1;
  int64_t max_video_width = std::max(max_video_width_, static_cast<int64_t>(video_frame_width_list_[0]));
  int64_t max_video_height = std::max(max_video_height_, static_cast<int64_t>(video_frame_height_list_[0]));
  int64_t max_video_frame_rate = std::max(max_video_frame_rate_, static_cast<int64_t>(video_frame_rate_list_[0]));

  bool meets_width = max_video_width >= layer_width;
  bool meets_height = max_video_height >= layer_height;

  bool meets_resolution = max_resolution_not_set || meets_width || meets_height;
  bool meets_frame_rate = max_frame_rate_not_set || max_video_frame_rate >= layer_frame_rate;

  return meets_resolution && meets_frame_rate;
}

void QualityManager::selectLayer(bool try_higher_layers) {
  if (!stats_ || !stats_->getNode().hasChild("qualityLayers")) {
    return;
  }
  last_quality_check_ = clock_->now();
  int aux_temporal_layer = 0;
  int aux_spatial_layer = 0;
  int next_temporal_layer = 0;
  int next_spatial_layer = 0;
  float bitrate_margin = try_higher_layers ? kIncreaseLayerBitrateThreshold : 0;
  bool below_min_layer = true;
  bool layer_capped_by_constraints = false;
  ELOG_DEBUG("Calculate best layer with %lu, current layer %d/%d",
      current_estimated_bitrate_, spatial_layer_, temporal_layer_);
  for (auto &spatial_layer_node : stats_->getNode()["qualityLayers"].getMap()) {
    for (auto &temporal_layer_node : spatial_layer_node.second->getMap()) {
      ELOG_DEBUG("Bitrate for layer %d/%d %lu",
          aux_spatial_layer, aux_temporal_layer, temporal_layer_node.second->value());
      if (temporal_layer_node.second->value() != 0 &&
          (1. + bitrate_margin) * temporal_layer_node.second->value() < current_estimated_bitrate_) {
        if (doesLayerMeetConstraints(aux_spatial_layer, aux_temporal_layer)) {
          next_temporal_layer = aux_temporal_layer;
          next_spatial_layer = aux_spatial_layer;
          below_min_layer = false;
        } else {
          layer_capped_by_constraints = true;
        }
      }
      aux_temporal_layer++;
    }
    aux_temporal_layer = 0;
    aux_spatial_layer++;
  }

  if (below_min_layer != slideshow_mode_active_) {
    if (below_min_layer || try_higher_layers) {
      slideshow_mode_active_ = below_min_layer;
      ELOG_DEBUG("Slideshow fallback mode %d", slideshow_mode_active_);
      WebRtcConnection *connection = getContext()->getPipelineShared()->getService<WebRtcConnection>().get();
      if (connection) {
        connection->notifyUpdateToHandlers();
      }
    }
  }

  if (next_temporal_layer != temporal_layer_ || next_spatial_layer != spatial_layer_) {
    ELOG_DEBUG("message: Layer Switch, current_layer: %d/%d, new_layer: %d/%d",
        spatial_layer_, temporal_layer_, next_spatial_layer, next_temporal_layer);
    setTemporalLayer(next_temporal_layer);
    setSpatialLayer(next_spatial_layer);

    // TODO(javier): should we wait for the actual spatial switch?
    // should we disable padding temporarily to avoid congestion (old padding + new bitrate)?
    setPadding(!isInMaxLayer() && !layer_capped_by_constraints);
    ELOG_DEBUG("message: Is padding enabled, padding_enabled_: %d", padding_enabled_);
  }
}

void QualityManager::calculateMaxActiveLayer() {
  int max_active_spatial_layer = 5;
  int max_active_temporal_layer = 5;

  for (; max_active_spatial_layer > 0; max_active_spatial_layer--) {
    if (getInstantLayerBitrate(max_active_spatial_layer, 0) > 0) {
      break;
    }
  }
  for (; max_active_temporal_layer > 0; max_active_temporal_layer--) {
    if (getInstantLayerBitrate(max_active_spatial_layer, max_active_temporal_layer) > 0) {
      break;
    }
  }
  stats_->getNode()["qualityLayers"].insertStat("maxActiveSpatialLayer",
      CumulativeStat{static_cast<uint64_t>(max_active_spatial_layer_)});
  stats_->getNode()["qualityLayers"].insertStat("maxActiveTemporalLayer",
      CumulativeStat{static_cast<uint64_t>(max_active_temporal_layer_)});

  max_active_spatial_layer_ = max_active_spatial_layer;
  max_active_temporal_layer_ = max_active_temporal_layer;
}

uint64_t QualityManager::getInstantLayerBitrate(int spatial_layer, int temporal_layer) {
  if (!stats_->getNode()["qualityLayers"].hasChild(spatial_layer) ||
      !stats_->getNode()["qualityLayers"][spatial_layer].hasChild(temporal_layer)) {
    return 0;
  }

  MovingIntervalRateStat* layer_stat =
    reinterpret_cast<MovingIntervalRateStat*>(&stats_->getNode()["qualityLayers"][spatial_layer][temporal_layer]);
  return layer_stat->value(kActiveLayerInterval);
}

bool QualityManager::isInBaseLayer() {
  return (spatial_layer_ == 0 && temporal_layer_ == 0);
}

bool QualityManager::isInMaxLayer() {
  return (spatial_layer_ >= max_active_spatial_layer_ && temporal_layer_ >= max_active_temporal_layer_);
}

void QualityManager::forceLayers(int spatial_layer, int temporal_layer) {
  if (spatial_layer == -1 || temporal_layer == -1) {
    forced_layers_ = false;
    return;
  }
  forced_layers_ = true;
  setPadding(false);

  spatial_layer_ = spatial_layer;
  temporal_layer_ = temporal_layer;
}

void QualityManager::setVideoConstraints(int max_video_width, int max_video_height, int max_video_frame_rate) {
  ELOG_DEBUG("Max: width (%d), height (%d), frameRate (%d)", max_video_width, max_video_height, max_video_frame_rate);

  max_video_width_ = max_video_width;
  max_video_height_ = max_video_height;
  max_video_frame_rate_ = max_video_frame_rate;
  selectLayer(true);
}

void QualityManager::setSpatialLayer(int spatial_layer) {
  if (!forced_layers_) {
    spatial_layer_ = spatial_layer;
  }
  stats_->getNode()["qualityLayers"].insertStat("selectedSpatialLayer",
      CumulativeStat{static_cast<uint64_t>(spatial_layer_)});
}
void QualityManager::setTemporalLayer(int temporal_layer) {
  if (!forced_layers_) {
    temporal_layer_ = temporal_layer;
  }
  stats_->getNode()["qualityLayers"].insertStat("selectedTemporalLayer",
      CumulativeStat{static_cast<uint64_t>(temporal_layer_)});
}

void QualityManager::setPadding(bool enabled) {
  if (padding_enabled_ != enabled) {
    padding_enabled_ = enabled;
    getContext()->getPipelineShared()->getService<WebRtcConnection>()->notifyUpdateToHandlers();
  }
}

}  // namespace erizo
