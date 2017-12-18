#ifndef ERIZO_SRC_ERIZO_RTP_RTCPNEWNACKGENERATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPNEWNACKGENERATOR_H_

#include "./logger.h"
#include "MediaDefinitions.h"
#include <boost/optional.hpp>
#include <set>
#include <memory>
#include <utility>
#include <chrono>  // NOLINT

namespace erizo {
namespace constants {
  constexpr int max_nack_list_size = 200;  // chrome default 250
  constexpr int max_packet_age_to_nack = 300;  // chrome default 450
}

inline bool is_newer_sequence_number(uint16_t sequence_number, uint16_t prev_sequence_number) {
  return sequence_number != prev_sequence_number &&
    static_cast<uint16_t>(sequence_number - prev_sequence_number) < 0x8000;
}

inline uint16_t latest_sequence_number(uint16_t sequence_number1, uint16_t sequence_number2) {
  return is_newer_sequence_number(sequence_number1, sequence_number2)
      ? sequence_number1
      : sequence_number2;
}

/// Comparator for seq_number_set
struct Seq_number_less_than {
  bool operator()(uint16_t lhs, uint16_t rhs) const { return is_newer_sequence_number(rhs, lhs); }
};

/// Set storing sequence numbers in ascending order (iff close numbers get inserted)
using Seq_number_set = std::set<uint16_t, Seq_number_less_than>;

class RtcpNewNackGenerator {
 public:
  DECLARE_LOGGER();

  explicit RtcpNewNackGenerator(uint32_t ssrc, const std::shared_ptr<Clock>& clock = std::make_shared<SteadyClock>())
  : ssrc_(ssrc), clock_(clock) {}

  /**
   * Handles an incoming rtp packet. Compatible with RtcpFeedbackGenerationHandler.
   *
   * @return A pair where first indicates if there's a NACK request and second if there's a PLI request
   */
  std::pair<bool, bool> handleRtpPacket(const std::shared_ptr<DataPacket>& packet);

  /**
   * Builds a nack and attaches it to an rtcp rr. Compatible with RtcpFeedbackGenerationHandler.
   */
  bool addNackPacketToRr(std::shared_ptr<DataPacket>& rr_packet);  // NOLINT

 private:
  inline bool too_large_nack_list() const;

  bool missing_too_old_packet(const uint16_t latest_sequence_number);

  bool handle_too_large_nack_list();

  bool handle_too_old_packets(const uint16_t latest_sequence_number);

  bool recycle_frames_until_key_frame();

  bool update_nack_list(const uint16_t seq);

  bool is_time_to_send(clock::time_point) const;

  std::chrono::milliseconds rtcp_min_interval() const;

  /**
   * Find and remove the oldest key frame from the set
   * @returns {true, sequence_number} if one key frame has been found, otherwise {false, undefined}
   */
  std::pair<bool, uint16_t> extract_oldest_keyframe();

  uint32_t ssrc_;
  boost::optional<uint16_t> latest_received_sequence_number_;
  Seq_number_set missing_sequence_numbers_;
  Seq_number_set keyframe_sequence_numbers_;

  std::shared_ptr<Clock> clock_;
  clock::time_point rtcp_send_time_;
};

bool RtcpNewNackGenerator::too_large_nack_list() const {
  return missing_sequence_numbers_.size() > constants::max_nack_list_size;
}
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPNEWNACKGENERATOR_H_
