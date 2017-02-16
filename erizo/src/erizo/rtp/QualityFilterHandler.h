#ifndef ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_

#include <memory>
#include <string>
#include <random>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"

#define MAX_DELAY 450000

namespace erizo {

class WebRtcConnection;


class QualityFilterHandler: public Handler, public std::enable_shared_from_this<QualityFilterHandler> {
  DECLARE_LOGGER();


 public:
  QualityFilterHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "quality_filter";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  WebRtcConnection *connection_;
  bool enabled_;
  bool initialized_;
  uint16_t last_seq_number_;
  int target_spatial_layer_;
  int target_temporal_layer_;
  uint video_sink_ssrc_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_
