#ifndef ERIZO_SRC_ERIZO_RTP_RTPAUDIOMUTEHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPAUDIOMUTEHANDLER_H_

#include "./logger.h"
#include "pipeline/Handler.h"

#include <mutex>  // NOLINT

namespace erizo {

class WebRtcConnection;

class RtpAudioMuteHandler: public Handler {
  DECLARE_LOGGER();

 public:
  RtpAudioMuteHandler();
  void muteAudio(bool active);

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "audio-mute";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  int32_t  last_original_seq_num_;
  uint16_t seq_num_offset_, last_sent_seq_num_;

  bool mute_is_active_;

  WebRtcConnection* connection_;

  inline void setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPAUDIOMUTEHANDLER_H_
