#ifndef ERIZO_SRC_ERIZO_RTP_RTCPRRGENERATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPRRGENERATOR_H_

#include <memory>
#include <string>
#include <random>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "lib/ClockUtils.h"

#define MAX_DELAY 450000

namespace erizo {

class RtcpRrGenerator {
  DECLARE_LOGGER();


 public:
  explicit RtcpRrGenerator(uint32_t ssrc, packetType type,
      std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());

  explicit RtcpRrGenerator(const RtcpRrGenerator&& handler);  // NOLINT
  bool handleRtpPacket(std::shared_ptr<DataPacket> packet);
  void handleSr(std::shared_ptr<DataPacket> packet);
  std::shared_ptr<DataPacket> generateReceiverReport();

 private:
  bool isRetransmitOfOldPacket(std::shared_ptr<DataPacket> packet);
  int getAudioClockRate(uint8_t payload_type);
  int getVideoClockRate(uint8_t payload_type);
  uint16_t selectInterval();
  uint16_t getRandomValue(uint16_t min, uint16_t max);

 private:
  class Jitter {
   public:
     Jitter() : transit_time(0), jitter(0) {}
     int transit_time;
     double jitter;
  };

  class RrPacketInfo {
   public:
     RrPacketInfo() : last_sr_ts{0}, next_packet_ms{0}, last_packet_ms{0}, ssrc{0}, last_sr_mid_ntp{0}, last_rr_ts{0},
       last_rtp_ts{0}, packets_received{0}, extended_seq{0}, lost{0}, expected_prior{0}, received_prior{0},
       last_recv_ts{0}, max_seq{-1}, base_seq{-1}, cycle{0}, frac_lost{0} {}
     explicit RrPacketInfo(uint32_t rr_ssrc) : last_sr_ts{0}, next_packet_ms{0}, last_packet_ms{0}, ssrc{rr_ssrc},
       last_sr_mid_ntp{0}, last_rr_ts{0}, last_rtp_ts{0}, packets_received{0}, extended_seq{0}, lost{0},
       expected_prior{0}, received_prior{0}, last_recv_ts{0}, max_seq{-1}, base_seq{-1}, cycle{0},
       frac_lost{0} {}
     uint64_t last_sr_ts, next_packet_ms, last_packet_ms;
     uint32_t ssrc, last_sr_mid_ntp, last_rr_ts, last_rtp_ts,
              packets_received, extended_seq, lost, expected_prior, received_prior, last_recv_ts;
     int32_t max_seq, base_seq;  // are really uint16_t, we're using the sign for unitialized values
     uint16_t cycle;
     uint8_t frac_lost;
     Jitter jitter;
  };

  uint8_t packet_[128];
  RrPacketInfo rr_info_;
  uint32_t ssrc_;
  packetType type_;
  std::random_device random_device_;
  std::mt19937 random_generator_;
  std::shared_ptr<Clock> clock_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPRRGENERATOR_H_
