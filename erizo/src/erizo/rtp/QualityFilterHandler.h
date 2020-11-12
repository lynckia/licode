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
  void sendPLI(packetPriority priority = HIGH_PRIORITY);
  void checkLayers();
  void handleFeedbackPackets(const std::shared_ptr<DataPacket> &packet);
  bool checkSSRCChange(uint32_t ssrc);
  void changeSpatialLayerOnKeyframeReceived(const std::shared_ptr<DataPacket> &packet);
  void detectVideoScalability(const std::shared_ptr<DataPacket> &packet);
  void updatePictureID(const std::shared_ptr<DataPacket> &packet, int new_picture_id);
  void updateTL0PicIdx(const std::shared_ptr<DataPacket> &packet, uint8_t new_tl0_pic_idx);
  void removeVP8OptionalPayload(const std::shared_ptr<DataPacket> &packet);

 private:
  std::shared_ptr<QualityManager> quality_manager_;
  SequenceNumberTranslator translator_;
  SequenceNumberTranslator picture_id_translator_;
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
  uint32_t timestamp_offset_;
  time_point time_change_started_;
  uint8_t tl0_pic_idx_offset_;
  uint8_t last_tl0_pic_idx_sent_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_
