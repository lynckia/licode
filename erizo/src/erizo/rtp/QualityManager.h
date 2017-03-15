#ifndef ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_
#define ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_

#include "./logger.h"
#include "Stats.h"
#include "lib/Clock.h"
#include "pipeline/Service.h"

namespace erizo {

class QualityManager: public Service, public std::enable_shared_from_this<QualityManager> {
  DECLARE_LOGGER();

 public:
  static constexpr duration kMinLayerSwitchInterval = std::chrono::seconds(10);
  static constexpr duration kActiveLayerInterval = std::chrono::milliseconds(500);
  static constexpr float kIncreaseLayerBitrateThreshold = 0.1;

 public:
  explicit QualityManager(std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());

  virtual  int getSpatialLayer() const { return spatial_layer_; }
  virtual  int getTemporalLayer() const { return temporal_layer_; }
  void setSpatialLayer(int spatial_layer);
  void setTemporalLayer(int temporal_layer);

  void forceLayers(int spatial_layer, int temporal_layer);

  void notifyQualityUpdate();

  virtual bool isPaddingEnabled() const { return padding_enabled_; }

 private:
  void calculateMaxActiveLayer();
  void selectLayer(bool try_higher_layers);
  uint64_t getInstantLayerBitrate(int spatial_layer, int temporal_layer);
  bool isInBaseLayer();
  bool isInMaxLayer();


 private:
  bool initialized_;
  bool padding_enabled_;
  bool forced_layers_;
  int spatial_layer_;
  int temporal_layer_;
  int max_active_spatial_layer_;
  int max_active_temporal_layer_;
  uint64_t current_estimated_bitrate_;

  time_point last_quality_check_;
  time_point last_activity_check_;
  std::shared_ptr<Stats> stats_;
  std::shared_ptr<Clock> clock_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_
