#ifndef ERIZO_SRC_ERIZO_RTP_RRGENERATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RRGENERATIONHANDLER_H_

#include <memory>
#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"

namespace erizo {

class WebRtcConnection;

class RRGenerationHandler: public Handler {
  DECLARE_LOGGER();

  struct RRPackets {
    RRPackets() : ssrc(0), max_seq(0), cycle(0), last_sr_mid_ntp(0), last_sr_recv_ts(0), last_rr_sent_ts(0),
                  last_rtp_ts(0), base_seq(0), p_received(0), extended_max(0), lost(0), expected_prior(0),
                  received_prior(0), frac_lost(0) {}
    uint32_t ssrc, max_seq, cycle, last_sr_mid_ntp, last_sr_recv_ts, last_rr_sent_ts, last_rtp_ts,
             base_seq, p_received, extended_max, lost, expected_prior, received_prior, frac_lost;
  } audio_rr_, video_rr_;

  struct Jitter {
    Jitter() : transit(0), jitter(0) {}
    int transit;
    double jitter;
  } jitter_video_, jitter_audio_;

 public:
  explicit RRGenerationHandler(WebRtcConnection *connection);

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "rr_generation";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;

 private:
  WebRtcConnection *connection_;
  Context *temp_ctx_;
  uint8_t packet_[128];
  bool enabled_;

  bool rtpSequenceLessThan(uint16_t x, uint16_t y);
  void handleRtpPacket(std::shared_ptr<dataPacket> packet);
  void handleSR(std::shared_ptr<dataPacket> packet);
  int getAudioClockRate(uint8_t payloadType);
  int getVideoClockRate(uint8_t payloadType);
  void sendVideoRR();
  void sendAudioRR();
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RRGENERATIONHANDLER_H_
