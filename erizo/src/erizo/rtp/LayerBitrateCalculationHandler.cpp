#include "rtp/LayerBitrateCalculationHandler.h"

#include <vector>

#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(LayerBitrateCalculationHandler, "rtp.LayerBitrateCalculationHandler");

LayerBitrateCalculationHandler::LayerBitrateCalculationHandler() : connection_{nullptr}, enabled_{true},
  initialized_{false} {}

void LayerBitrateCalculationHandler::enable() {
  enabled_ = true;
}

void LayerBitrateCalculationHandler::disable() {
  enabled_ = false;
}

void LayerBitrateCalculationHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
    std::for_each(packet->compatible_spatial_layers.begin(),
        packet->compatible_spatial_layers.end(), [](int &layer_num){
        // Add spatial_num to stats if it doesn't exist
        // sum+ packet size
        });
    std::for_each(packet->compatible_temporal_layers.begin(),
        packet->compatible_temporal_layers.end(), [](int &layer_num){
        // Add spatial_num to stats if it doesn't exist
        // sum+ packet size
        });
}


void LayerBitrateCalculationHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  connection_ = pipeline->getService<WebRtcConnection>().get();
  if (!connection_) {
    return;
  }
}
}  // namespace erizo
