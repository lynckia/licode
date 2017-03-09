#include "rtp/QualityManager.h"

namespace erizo {

DEFINE_LOGGER(QualityManager, "rtp.QualityManager");

QualityManager::QualityManager()
  : spatial_layer_{0}, temporal_layer_{0}, padding_enabled_{false} {}

}  // namespace erizo
