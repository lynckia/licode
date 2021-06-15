#ifndef ERIZO_SRC_ERIZO_RTP_STREAMSWITCHHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_STREAMSWITCHHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/SequenceNumberTranslator.h"
#include "lib/ClockUtils.h"

namespace erizo {

class MediaStream;

class TrackState {
 public:
  TrackState() :
    switched{false},
    timestamp_offset{0},
    last_timestamp_sent{0},
    last_timestamp_sent_at{0},
    tl0_pic_idx_offset{0},
    last_tl0_pic_idx_sent{0},
    clock_rate{1},
    keyframe_received{false},
    frame_received{false} {}

 public:
  SequenceNumberTranslator sequence_number_translator;
  SequenceNumberTranslator picture_id_translator;
  bool switched;
  uint32_t timestamp_offset;
  uint32_t last_timestamp_sent;
  uint32_t last_timestamp_sent_at;
  uint8_t tl0_pic_idx_offset;
  uint8_t last_tl0_pic_idx_sent;
  uint32_t clock_rate;
  std::shared_ptr<DataPacket> last_packet;
  bool keyframe_received;
  bool frame_received;
};

class StreamSwitchHandler: public Handler, public std::enable_shared_from_this<StreamSwitchHandler> {
  DECLARE_LOGGER();

 public:
  explicit StreamSwitchHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "stream-switch-handler";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;
  void notifyEvent(MediaEventPtr event) override;

 private:
  std::shared_ptr<TrackState> getStateForSsrc(uint32_t ssrc, bool should_create);
  void storeLastPacket(const std::shared_ptr<TrackState> &state, const std::shared_ptr<DataPacket> &packet);
  void sendBlackKeyframe(std::shared_ptr<DataPacket> packet);
  void sendPLI();
  void schedulePLI();
  uint32_t getNow();

 private:
  MediaStream* stream_;
  std::map<uint32_t, std::shared_ptr<TrackState>> state_map_;
  uint32_t video_ssrc_;
  bool generate_video_;
  unsigned int video_pt_;
  std::string video_codec_name_;
  unsigned int video_clock_rate_;
  uint16_t generated_seq_number_;
  std::shared_ptr<erizo::Clock> clock_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_STREAMSWITCHHANDLER_H_
