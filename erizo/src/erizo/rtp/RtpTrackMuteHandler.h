#ifndef ERIZO_SRC_ERIZO_RTP_RTPTRACKMUTEHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPTRACKMUTEHANDLER_H_

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/SequenceNumberTranslator.h"

#include <mutex>  // NOLINT

static constexpr erizo::duration kMuteVideoKeyframeTimeout = std::chrono::seconds(5);

namespace erizo {

class MediaStream;

class TrackMuteInfo {
 public:
  explicit TrackMuteInfo(std::string label_)
    : label{label_}, highest_seq_num{0}, highest_seq_num_initialized{false},
    mute_is_active{false}, unmute_requested{false} {}
  std::string label;
  SequenceNumberTranslator translator;
  uint16_t highest_seq_num;
  uint16_t highest_seq_num_initialized;
  bool mute_is_active;
  bool unmute_requested;
};

class RtpTrackMuteHandler: public Handler {
  DECLARE_LOGGER();

 public:
  explicit RtpTrackMuteHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<SteadyClock>());
  void muteAudio(bool active);
  void muteVideo(bool active);

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "track-mute";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void muteTrack(TrackMuteInfo *info, bool active);
  void handleFeedback(const TrackMuteInfo &info, const std::shared_ptr<DataPacket> &packet);
  void handlePacket(Context *ctx, TrackMuteInfo *info, std::shared_ptr<DataPacket> packet);
  void maybeUpdateHighestSeqNum(TrackMuteInfo *info, uint16_t seq_num);
  inline void setPacketSeqNumber(std::shared_ptr<DataPacket> packet, uint16_t seq_number);
  std::shared_ptr<DataPacket> transformIntoBlackKeyframePacket(std::shared_ptr<DataPacket> packet);
  void updateOffset(TrackMuteInfo *info);

 private:
  TrackMuteInfo audio_info_;
  TrackMuteInfo video_info_;
  time_point last_keyframe_sent_time_;
  std::shared_ptr<erizo::Clock> clock_;

  MediaStream* stream_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPTRACKMUTEHANDLER_H_
