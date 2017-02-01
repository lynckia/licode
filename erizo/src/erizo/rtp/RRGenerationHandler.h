#ifndef ERIZO_SRC_ERIZO_RTP_RRGENERATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RRGENERATIONHANDLER_H_

#include <memory>
#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"

#define MAX_DELAY 450000

namespace erizo {

class WebRtcConnection;

class RRGenerationHandler: public Handler {
  DECLARE_LOGGER();


 public:
  explicit RRGenerationHandler(WebRtcConnection *connection);

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "rr_generation";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  struct RRPackets {
    RRPackets() : last_sr_ts{0}, ssrc{0}, last_sr_mid_ntp{0}, last_rr_ts{0},
      last_rtp_ts{0}, p_received{0}, extended_seq{0}, lost{0}, expected_prior{0}, received_prior{0},
      last_recv_ts{0}, max_seq{-1}, base_seq{-1}, cycle{0}, frac_lost{0} {}
    uint64_t last_sr_ts;
    uint32_t ssrc, last_sr_mid_ntp, last_rr_ts, last_rtp_ts,
             p_received, extended_seq, lost, expected_prior, received_prior, last_recv_ts;
    int32_t max_seq, base_seq;  // are really uint16_t, we're using the sign for unitialized values
    uint16_t cycle;
    uint8_t frac_lost;
  };

  struct Jitter {
    Jitter() : transit_time(0), jitter(0) {}
    int transit_time;
    double jitter;
  };

  Context *temp_ctx_;
  uint8_t packet_[128];
  bool enabled_;
  RRPackets audio_rr_, video_rr_;
  Jitter jitter_video_, jitter_audio_;

  bool rtpSequenceLessThan(uint16_t x, uint16_t y);
  bool isRetransmitOfOldPacket(std::shared_ptr<dataPacket> packet);
  void handleRtpPacket(std::shared_ptr<dataPacket> packet);
  void handleSR(std::shared_ptr<dataPacket> packet);
  int getAudioClockRate(uint8_t payload_type);
  int getVideoClockRate(uint8_t payload_type);
  void sendVideoRR();
  void sendAudioRR();
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RRGENERATIONHANDLER_H_
