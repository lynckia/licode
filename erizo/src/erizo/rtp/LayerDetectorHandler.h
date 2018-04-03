#ifndef ERIZO_SRC_ERIZO_RTP_LAYERDETECTORHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_LAYERDETECTORHANDLER_H_

#include <memory>
#include <string>
#include <random>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtpVP9Parser.h"
#include "rtp/RtpH264Parser.h"
#include "./Stats.h"

#define MAX_DELAY 450000

namespace erizo {

class LayerInfoChangedEvent : public MediaEvent {
 public:
  LayerInfoChangedEvent(std::vector<uint32_t> video_frame_width_list_, std::vector<uint32_t> video_frame_height_list_,
                        std::vector<uint64_t> video_frame_rate_list_)
    : video_frame_width_list{video_frame_width_list_},
      video_frame_height_list{video_frame_height_list_},
      video_frame_rate_list{video_frame_rate_list_} {}

  std::string getType() const override {
    return "LayerInfoChangedEvent";
  }
  std::vector<uint32_t> video_frame_width_list;
  std::vector<uint32_t> video_frame_height_list;
  std::vector<uint64_t> video_frame_rate_list;
};

class MediaStream;

class LayerDetectorHandler: public InboundHandler, public std::enable_shared_from_this<LayerDetectorHandler> {
  DECLARE_LOGGER();


 public:
  explicit LayerDetectorHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<erizo::SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "layer_detector";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void parseLayerInfoFromVP8(std::shared_ptr<DataPacket> packet);
  void parseLayerInfoFromVP9(std::shared_ptr<DataPacket> packet);
  void parseLayerInfoFromH264(std::shared_ptr<DataPacket> packet);
  int getSsrcPosition(uint32_t ssrc);
  void addTemporalLayerAndCalculateRate(const std::shared_ptr<DataPacket> &packet, int temporal_layer, bool new_frame);
  void notifyLayerInfoChangedEvent();
  void notifyLayerInfoChangedEventMaybe();

 private:
  std::shared_ptr<erizo::Clock> clock_;
  MediaStream *stream_;
  bool enabled_;
  bool initialized_;
  RtpVP8Parser vp8_parser_;
  RtpVP9Parser vp9_parser_;
  RtpH264Parser h264_parser_;
  std::vector<uint32_t> video_ssrc_list_;
  std::vector<uint32_t> video_frame_height_list_;
  std::vector<uint32_t> video_frame_width_list_;
  std::vector<MovingIntervalRateStat> video_frame_rate_list_;
  std::chrono::steady_clock::time_point last_event_sent_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_LAYERDETECTORHANDLER_H_
