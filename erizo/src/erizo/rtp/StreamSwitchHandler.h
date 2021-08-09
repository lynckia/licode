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
    initialized{false},
    timestamp_offset{0},
    last_timestamp_sent{0},
    last_timestamp_received{0},
    last_timestamp_sent_at{0},
    switch_called_at{0},
    tl0_pic_idx_offset{0},
    last_tl0_pic_idx_sent{0},
    last_tl0_pic_idx_received{0},
    last_picture_id_sent{0},
    last_picture_id_received{0},
    clock_rate{1},
    keyframe_received{false},
    frame_received{false},
    last_sequence_number_sent{0},
    last_sequence_number_received{0},
    time_with_silence{0} {}

 public:
  SequenceNumberTranslator sequence_number_translator;
  SequenceNumberTranslator picture_id_translator;
  bool switched;
  bool initialized;
  uint32_t timestamp_offset;
  uint32_t last_timestamp_sent;
  uint32_t last_timestamp_received;
  uint32_t last_timestamp_sent_at;
  uint32_t switch_called_at;
  uint8_t tl0_pic_idx_offset;
  uint8_t last_tl0_pic_idx_sent;
  uint8_t last_tl0_pic_idx_received;
  uint16_t last_picture_id_sent;
  uint16_t last_picture_id_received;
  uint16_t picture_id_offset;
  uint32_t clock_rate;
  std::shared_ptr<DataPacket> last_packet;
  bool keyframe_received;
  bool frame_received;
  uint16_t last_sequence_number_sent;
  uint16_t last_sequence_number_received;
  uint32_t time_with_silence;
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
  void sendBlackKeyframe(std::shared_ptr<DataPacket> packet, int additional, uint32_t clock_rate, const std::shared_ptr<TrackState> &state);
  void handleFeedbackPackets(const std::shared_ptr<DataPacket> &packet);
  void sendPLI();
  void schedulePLI();
  uint32_t getNow();
  void updatePictureID(const std::shared_ptr<DataPacket> &packet, int new_picture_id);
  void updateTL0PicIdx(const std::shared_ptr<DataPacket> &packet, uint8_t new_tl0_pic_idx);

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
  uint32_t fir_seq_number_;
  bool enable_plis_;
  bool plis_scheduled_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_STREAMSWITCHHANDLER_H_
