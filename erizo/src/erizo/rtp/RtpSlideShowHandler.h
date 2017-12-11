#ifndef ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_

#include <string>

#include "pipeline/Handler.h"
#include "./logger.h"
#include "rtp/SequenceNumberTranslator.h"
#include "rtp/PacketBufferService.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtpVP9Parser.h"
#include "lib/ClockUtils.h"

static constexpr uint16_t kMaxKeyframeSize = 20;
static constexpr erizo::duration kFallbackKeyframeTimeout = std::chrono::seconds(5);

namespace erizo {

class MediaStream;

class RtpSlideShowHandler : public Handler {
  DECLARE_LOGGER();

 public:
  explicit RtpSlideShowHandler(std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "slideshow";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

  void setSlideShowMode(bool activated);

 private:
  bool isVP8OrH264Keyframe(std::shared_ptr<DataPacket> packet);
  bool isVP9Keyframe(std::shared_ptr<DataPacket> packet);
  void maybeUpdateHighestSeqNum(uint16_t seq_num);
  void resetKeyframeBuilding();
  void consolidateKeyframe();
  void maybeSendStoredKeyframe();
  void storeKeyframePacket(std::shared_ptr<DataPacket> packet);

 private:
  std::shared_ptr<Clock> clock_;
  MediaStream* stream_;
  SequenceNumberTranslator translator_;
  bool highest_seq_num_initialized_;
  bool is_building_keyframe_;
  uint16_t  highest_seq_num_;
  uint16_t packets_received_while_building_;
  uint16_t first_keyframe_seq_num_;
  bool slideshow_is_active_;
  uint32_t current_keyframe_timestamp_;
  uint32_t last_timestamp_received_;

  std::vector<std::shared_ptr<DataPacket>> keyframe_buffer_;
  std::vector<std::shared_ptr<DataPacket>> stored_keyframe_;
  time_point last_keyframe_sent_time_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_
