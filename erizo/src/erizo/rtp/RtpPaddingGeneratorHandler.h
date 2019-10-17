#ifndef ERIZO_SRC_ERIZO_RTP_RTPPADDINGGENERATORHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPPADDINGGENERATORHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "lib/Clock.h"
#include "lib/TokenBucket.h"
#include "thread/Worker.h"
#include "rtp/SequenceNumberTranslator.h"
#include "./Stats.h"

namespace erizo {

class MediaStream;

class RtpPaddingGeneratorHandler: public Handler, public std::enable_shared_from_this<RtpPaddingGeneratorHandler> {
  DECLARE_LOGGER();

 public:
  explicit RtpPaddingGeneratorHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<erizo::SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "padding-generator";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void sendPaddingPacket(std::shared_ptr<DataPacket> packet, uint8_t padding_size);
  void onPacketWithMarkerSet(std::shared_ptr<DataPacket> packet);
  bool isHigherSequenceNumber(std::shared_ptr<DataPacket> packet);
  void onVideoPacket(std::shared_ptr<DataPacket> packet);

  uint64_t getBurstSize();

  void recalculatePaddingRate(uint64_t target_padding_bitrate);

  void enablePadding();
  void disablePadding();

 private:
  std::shared_ptr<erizo::Clock> clock_;
  SequenceNumberTranslator translator_;
  MediaStream* stream_;
  std::shared_ptr<Stats> stats_;
  uint16_t higher_sequence_number_;
  uint32_t video_sink_ssrc_;
  uint32_t audio_source_ssrc_;
  uint64_t number_of_full_padding_packets_;
  uint8_t last_padding_packet_size_;
  time_point started_at_;
  bool enabled_;
  bool first_packet_received_;
  bool slideshow_mode_active_;
  MovingIntervalRateStat marker_rate_;
  uint32_t rtp_header_length_;
  TokenBucket bucket_;
  std::shared_ptr<ScheduledTaskReference> scheduled_task_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPPADDINGGENERATORHANDLER_H_
