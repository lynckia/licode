#ifndef ERIZO_SRC_ERIZO_RTP_RTPTRACKMUTEHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPTRACKMUTEHANDLER_H_

#include "./logger.h"
#include "pipeline/Handler.h"

#include <mutex>  // NOLINT

namespace erizo {

class WebRtcConnection;

class TrackMuteInfo {
 public:
  explicit TrackMuteInfo(std::string label_)
    : label{label_}, last_original_seq_num{-1}, seq_num_offset{0},
      last_sent_seq_num{0}, mute_is_active{false} {}
  std::string label;
  int32_t last_original_seq_num;
  uint16_t seq_num_offset;
  uint16_t last_sent_seq_num;
  bool mute_is_active;
};

class RtpTrackMuteHandler: public Handler {
  DECLARE_LOGGER();

 public:
  RtpTrackMuteHandler();
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
  inline void setPacketSeqNumber(std::shared_ptr<DataPacket> packet, uint16_t seq_number);

 private:
  TrackMuteInfo audio_info_;
  TrackMuteInfo video_info_;

  WebRtcConnection* connection_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPTRACKMUTEHANDLER_H_
