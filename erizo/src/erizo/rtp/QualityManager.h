#ifndef ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_
#define ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_

#include "./logger.h"
#include "pipeline/Service.h"

namespace erizo {

class QualityManager: public Service, public std::enable_shared_from_this<QualityManager> {
  DECLARE_LOGGER();

 public:
  QualityManager();

  int getSpatialLayer() const { return spatial_layer_; }
  int getTemporalLayer() const { return temporal_layer_; }

  void setSpatialLayer(int spatial_layer) { spatial_layer_ = spatial_layer; }
  void setTemporalLayer(int temporal_layer) { temporal_layer_ = temporal_layer; }

 private:
  int spatial_layer_;
  int temporal_layer_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_QUALITYMANAGER_H_
