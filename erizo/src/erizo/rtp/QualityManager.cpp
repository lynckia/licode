#include "rtp/QualityManager.h"
#include <memory>

#include "./MediaStream.h"
#include "pipeline/HandlerManager.h"
#include "rtp/LayerDetectorHandler.h"

namespace erizo {

DEFINE_LOGGER(QualityManager, "rtp.QualityManager");

constexpr duration QualityManager::kMinLayerSwitchInterval;
constexpr duration QualityManager::kActiveLayerInterval;
constexpr float QualityManager::kIncreaseLayerBitrateThreshold;
constexpr duration QualityManager::kIncreaseConnectionQualityLevelInterval;

QualityManager::QualityManager(std::shared_ptr<Clock> the_clock)
  : initialized_{false}, enabled_{false}, padding_enabled_{false}, forced_layers_{false},
  freeze_fallback_active_{false}, enable_slideshow_below_spatial_layer_{false}, spatial_layer_{0},
  temporal_layer_{0}, max_active_spatial_layer_{0},
  max_active_temporal_layer_{0}, slideshow_below_spatial_layer_{-1}, max_video_width_{-1},
  max_video_height_{-1}, max_video_frame_rate_{-1}, current_estimated_bitrate_{0},
  last_quality_check_{the_clock->now()}, last_activity_check_{the_clock->now()}, clock_{the_clock},
  connection_quality_level_{ConnectionQualityLevel::GOOD}, connection_quality_level_updated_on_{the_clock->now()},
  last_connection_quality_level_received_{ConnectionQualityLevel::GOOD} {}

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
  } else if (event->getType() == "ConnectionQualityEvent") {
    auto quality_event = std::static_pointer_cast<ConnectionQualityEvent>(event);
    onConnectionQualityUpdate(quality_event->level);
  }
}

void QualityManager::onConnectionQualityUpdate(ConnectionQualityLevel level) {
  if (level == connection_quality_level_) {
    connection_quality_level_updated_on_ = clock_->now();
  } else if (level < connection_quality_level_) {
    connection_quality_level_ = level;
    connection_quality_level_updated_on_ = clock_->now();
    selectLayer(false);
  }
  last_connection_quality_level_received_ = level;
}

void QualityManager::checkIfConnectionQualityLevelIsBetterNow() {
  if (connection_quality_level_ < last_connection_quality_level_received_ &&
      (clock_->now() - connection_quality_level_updated_on_) > kIncreaseConnectionQualityLevelInterval) {
    connection_quality_level_ = ConnectionQualityLevel(connection_quality_level_ + 1);
    connection_quality_level_updated_on_ = clock_->now();
    selectLayer(true);
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
    stream_ = pipeline->getService<MediaStream>().get();
    if (!stream_) {
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

  if (!layer_is_active || (estimated_is_under_layer_bitrate && !freeze_fallback_active_)) {
    ELOG_DEBUG("message: Forcing calculate new layer, "
        "estimated_is_under_layer_bitrate: %d, layer_is_active: %d, freeze_fallback_active_: %d",
        estimated_is_under_layer_bitrate, layer_is_active, freeze_fallback_active_);
    selectLayer(false);
  } else if (now - last_quality_check_ > kMinLayerSwitchInterval) {
    selectLayer(true);
  }

  checkIfConnectionQualityLevelIsBetterNow();
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

void QualityManager::calculateMaxBitrateThatMeetsConstraints() {
  int max_available_spatial_layer_that_meets_constraints = 0;
  int max_available_temporal_layer_that_meets_constraints = 0;

  int max_spatial_layer_with_resolution_info = 0;
  if (video_frame_width_list_.size() > 0 && video_frame_height_list_.size() > 0) {
    max_spatial_layer_with_resolution_info = std::min(static_cast<int>(video_frame_width_list_.size()) - 1,
                                                      static_cast<int>(video_frame_height_list_.size()) - 1);
  }
  int max_temporal_layer_with_frame_rate_info = std::max(static_cast<int>(video_frame_rate_list_.size()) - 1, 0);

  int max_spatial_layer_available = std::min(max_spatial_layer_with_resolution_info, max_active_spatial_layer_);
  int max_temporal_layer_available = std::min(max_temporal_layer_with_frame_rate_info, max_active_temporal_layer_);

  for (int spatial_layer = 0; spatial_layer <= max_spatial_layer_available; spatial_layer++) {
    for (int temporal_layer = 0; temporal_layer <= max_temporal_layer_available; temporal_layer++) {
      if (doesLayerMeetConstraints(spatial_layer, temporal_layer)) {
        max_available_spatial_layer_that_meets_constraints = spatial_layer;
        max_available_temporal_layer_that_meets_constraints = temporal_layer;
      }
    }
  }

  stream_->setBitrateFromMaxQualityLayer(getInstantLayerBitrate(max_available_spatial_layer_that_meets_constraints,
                                                                max_available_temporal_layer_that_meets_constraints));
}

void QualityManager::selectLayer(bool try_higher_layers) {
  if (!initialized_  || !stats_->getNode().hasChild("qualityLayers")) {
    return;
  }
  stream_->setSimulcast(true);
  last_quality_check_ = clock_->now();
  calculateMaxBitrateThatMeetsConstraints();

  int min_requested_spatial_layer =
    enable_slideshow_below_spatial_layer_ ? std::max(slideshow_below_spatial_layer_, 0) : 0;
  int min_valid_spatial_layer = std::min(min_requested_spatial_layer, max_active_spatial_layer_);
  int aux_temporal_layer = 0;
  int aux_spatial_layer = 0;
  int next_temporal_layer = 0;
  int next_spatial_layer = min_valid_spatial_layer;
  float bitrate_margin = try_higher_layers ? kIncreaseLayerBitrateThreshold : 0;
  bool below_min_layer = true;
  bool layer_capped_by_constraints = false;
  ELOG_DEBUG("message: Calculate best layer, estimated_bitrate: %lu, current layer %d/%d, min_requested_spatial %d",
      current_estimated_bitrate_, spatial_layer_, temporal_layer_, min_requested_spatial_layer);
  for (auto &spatial_layer_node : stats_->getNode()["qualityLayers"].getMap()) {
    if (aux_spatial_layer >= min_valid_spatial_layer) {
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
    } else {
      ELOG_DEBUG("message: Skipping below min spatial layer, aux_layer: %d, min_valid_spatial_layer: %d",
          aux_spatial_layer, min_valid_spatial_layer);
    }
    aux_temporal_layer = 0;
    aux_spatial_layer++;
  }
  bool padding_disabled_by_bad_connection = false;
  if (!enable_slideshow_below_spatial_layer_ && connection_quality_level_ == ConnectionQualityLevel::GOOD) {
    below_min_layer = false;
  } else if (connection_quality_level_ == ConnectionQualityLevel::HIGH_LOSSES) {
    next_temporal_layer = 0;
    next_spatial_layer = 0;
    below_min_layer = true;
    padding_disabled_by_bad_connection = true;
  } else if (connection_quality_level_ == ConnectionQualityLevel::LOW_LOSSES) {
    // We'll enable fallback when needed by not updating below_min_layer to false
    padding_disabled_by_bad_connection = true;
  }

  ELOG_DEBUG("message: below_min_layer %u, freeze_fallback_active_: %u", below_min_layer, freeze_fallback_active_);
  if (below_min_layer != freeze_fallback_active_) {
    if (below_min_layer || try_higher_layers) {
      freeze_fallback_active_ = below_min_layer;
      ELOG_DEBUG("message: Setting slideshow fallback, below_min_layer %u, spatial_layer %d,"
          "next_spatial_layer %d freeze_fallback_active_: %d, min_requested_spatial_layer: %d,"
          "slideshow_below_spatial_layer_ %d",
          below_min_layer, spatial_layer_, next_spatial_layer, freeze_fallback_active_,
          min_requested_spatial_layer, slideshow_below_spatial_layer_);
      HandlerManager *manager = getContext()->getPipelineShared()->getService<HandlerManager>().get();
      if (manager) {
        manager->notifyUpdateToHandlers();
      }
      if (enable_slideshow_below_spatial_layer_ && slideshow_below_spatial_layer_ >= 0) {
        if (below_min_layer) {
          ELOG_DEBUG("message: Spatial layer is below min valid spatial layer %d and slideshow is requested "
              ", activating keyframe requests", min_valid_spatial_layer);
          stream_->notifyMediaStreamEvent("slideshow_fallback_update", "true");
        } else {
          ELOG_DEBUG("message: Spatial layer has recovered %d, deactivating keyframe requests",
              next_spatial_layer);
          stream_->notifyMediaStreamEvent("slideshow_fallback_update", "false");
        }
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
  }
  stats_->getNode()["qualityLayers"].insertStat("qualityCappedByConstraints",
                                                CumulativeStat{layer_capped_by_constraints});
  setPadding(!isInMaxLayer() && !layer_capped_by_constraints && !padding_disabled_by_bad_connection);
  ELOG_DEBUG("message: Is padding enabled, padding_enabled_: %d", padding_enabled_);
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

void QualityManager::enableSlideShowBelowSpatialLayer(bool enable, int spatial_layer) {
  ELOG_DEBUG("message: enableSlideShowBelowSpatialLayer, enable %d, spatial_layer: %d", enable, spatial_layer);
  enable_slideshow_below_spatial_layer_ = enable;
  slideshow_below_spatial_layer_ = spatial_layer;

  if (!initialized_) {
    return;
  }

  stream_->notifyMediaStreamEvent("slideshow_fallback_update", "false");
  freeze_fallback_active_ = false;

  selectLayer(true);
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
    HandlerManager *manager = getContext()->getPipelineShared()->getService<HandlerManager>().get();
    if (manager) {
      manager->notifyUpdateToHandlers();
    }
  }
}

void QualityManager::setConnectionQualityLevel(ConnectionQualityLevel level) {
  connection_quality_level_ = level;
  connection_quality_level_updated_on_ = clock_->now();
}

}  // namespace erizo
