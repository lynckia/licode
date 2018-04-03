#ifndef ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_

#include <memory>
#include <string>
#include <random>
#include <map>

#include "./logger.h"
#include "lib/Clock.h"
#include "pipeline/Handler.h"
#include "rtp/SequenceNumberTranslator.h"
#include "rtp/QualityManager.h"

namespace erizo {

class MediaStream;

class QualityFilterHandler: public Handler, public std::enable_shared_from_this<QualityFilterHandler> {
  DECLARE_LOGGER();


 public:
  QualityFilterHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "quality_filter";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void sendPLI();
  void checkLayers();
  void handleFeedbackPackets(const std::shared_ptr<DataPacket> &packet);
  bool checkSSRCChange(uint32_t ssrc);
  void changeSpatialLayerOnKeyframeReceived(const std::shared_ptr<DataPacket> &packet);
  void detectVideoScalability(const std::shared_ptr<DataPacket> &packet);
  void updatePictureID(const std::shared_ptr<DataPacket> &packet);

 private:
  std::shared_ptr<QualityManager> quality_manager_;
  SequenceNumberTranslator translator_;
  MediaStream *stream_;
  bool enabled_;
  bool initialized_;
  bool receiving_multiple_ssrc_;
  bool changing_spatial_layer_;
  bool is_scalable_;
  int target_spatial_layer_;
  int future_spatial_layer_;
  int target_temporal_layer_;
  uint32_t video_sink_ssrc_;
  uint32_t video_source_ssrc_;
  uint32_t last_ssrc_received_;
  uint32_t max_video_bw_;
  uint32_t last_timestamp_sent_;
  int64_t timestamp_offset_;
  time_point time_change_started_;
  int picture_id_offset_;
  int last_picture_id_sent_;
  uint32_t base_ts_ssrc = 0;
  std::map<uint32_t, uint32_t> last_rtcp_timestamp;
  std::map<uint32_t, int64_t> ntp_ms;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_
