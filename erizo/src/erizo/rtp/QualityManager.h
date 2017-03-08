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
  static constexpr duration kMinLayerChangeInterval = std::chrono::seconds(2);
  static constexpr duration kActiveLayerInterval = std::chrono::milliseconds(500);

 public:
  QualityManager();

  int getSpatialLayer() const { return spatial_layer_; }
  int getTemporalLayer() const { return temporal_layer_; }
  void setSpatialLayer(int spatial_layer)  {spatial_layer_ = spatial_layer;}
  void setTemporalLayer(int temporal_layer)  {temporal_layer_ = temporal_layer;}

  void notifyQualityUpdate();

 private:
  bool initialized_;
  int spatial_layer_;
  int temporal_layer_;
  std::string spatial_layer_str_;
  std::string temporal_layer_str_;
  uint64_t current_estimated_bitrate_;

  time_point last_quality_check_;
  std::shared_ptr<Stats> stats_;

  void selectLayer();
  bool isCurrentLayerPresent();
  bool isInBaseLayer();
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_
