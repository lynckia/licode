#ifndef ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_
#define ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_

#include "./logger.h"
#include "Stats.h"
#include "lib/Clock.h"
#include "pipeline/Service.h"

namespace erizo {

class MediaStream;

class QualityManager: public Service, public std::enable_shared_from_this<QualityManager> {
  DECLARE_LOGGER();

 public:
  static constexpr duration kMinLayerSwitchInterval = std::chrono::seconds(10);
  static constexpr duration kActiveLayerInterval = std::chrono::milliseconds(500);
  static constexpr float kIncreaseLayerBitrateThreshold = 0.1;

 public:
  explicit QualityManager(std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());
  void enable();
  void disable();

  virtual  int getSpatialLayer() const { return spatial_layer_; }
  virtual  int getTemporalLayer() const { return temporal_layer_; }
  virtual  bool isFallbackFreezeEnabled() const { return freeze_fallback_active_; }

  void setSpatialLayer(int spatial_layer);
  void setTemporalLayer(int temporal_layer);

  void forceLayers(int spatial_layer, int temporal_layer);
  void enableSlideShowBelowSpatialLayer(bool enabled, int spatial_layer);
  void enableFallbackBelowMinLayer(bool enabled);
  bool isEnableSlideshowBelowSpatialLayer() { return enable_slideshow_below_spatial_layer_; }
  bool isEnableFallbackBelowMinLayer() { return enable_fallback_below_min_layer_; }
  void setVideoConstraints(int max_video_width, int max_video_height, int max_video_frame_rate);
  void notifyEvent(MediaEventPtr event) override;
  void notifyQualityUpdate();

 private:
  void calculateMaxActiveLayer();
  void maybeUpdateAvailableLayersAndBitrates();
  void storeLayersAndBitratesInMediaStream();
  void selectLayer(bool try_higher_layers);
  uint64_t getInstantLayerBitrate(int spatial_layer, int temporal_layer);
  uint64_t getMaxLayerBitrateInInterval(int spatial_layer, int temporal_layer);
  bool isInBaseLayer();
  bool isInMaxLayer();
  bool doesLayerMeetConstraints(int spatial_layer, int temporal_layer);

 private:
  MediaStream* stream_;
  bool initialized_;
  bool enabled_;
  bool forced_layers_;
  bool freeze_fallback_active_;
  bool enable_slideshow_below_spatial_layer_;
  bool enable_fallback_below_min_layer_;
  int spatial_layer_;
  int temporal_layer_;
  int max_active_spatial_layer_;
  int max_active_temporal_layer_;
  int slideshow_below_spatial_layer_;
  int64_t max_video_width_;
  int64_t max_video_height_;
  int64_t max_video_frame_rate_;
  uint64_t current_estimated_bitrate_;

  time_point last_quality_check_;
  time_point last_activity_check_;
  std::shared_ptr<Stats> stats_;
  std::shared_ptr<Clock> clock_;
  std::vector<uint32_t> video_frame_width_list_;
  std::vector<uint32_t> video_frame_height_list_;
  std::vector<uint64_t> video_frame_rate_list_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_
