#include "rtp/RtcpNewNackGenerator.h"

#include <algorithm>
#include "rtp/RtpUtils.h"

namespace erizo {
namespace {
constexpr int max_nack_blocks = 10;
constexpr int kNackCommonHeaderLengthRtcp = kNackCommonHeaderLengthBytes / 4 - 1;
constexpr int nack_interval = 50;

/**
 * Drops from the set all packets older than the given sequence_number
 */
void drop_packets_from_set(Seq_number_set& set, uint16_t sequence_number) {  // NOLINT
  set.erase(set.begin(), set.upper_bound(sequence_number));
}

void attach_nack_to_rr(uint32_t ssrc,
    std::shared_ptr<DataPacket>& rr_packet,  // NOLINT
    const std::vector<NackBlock>& nack_blocks);

std::vector<NackBlock> build_nack_blocks(const Seq_number_set& set);
}  // namespace

DEFINE_LOGGER(RtcpNewNackGenerator, "rtp.RtcpNewNackGenerator");

std::pair<bool, bool> RtcpNewNackGenerator::handleRtpPacket(const std::shared_ptr<DataPacket>& packet) {
  if (packet->type != VIDEO_PACKET) {
    return {false, false};
  }
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  if (head->getSSRC() != ssrc_) {
    ELOG_DEBUG("message: handleRtpPacket Unknown SSRC, ssrc: %u", head->getSSRC());
    return {false, false};
  }
  const uint16_t seq_num = head->getSeqNumber();
  // Save when we got a key frame
  if (packet->is_keyframe) {
    keyframe_sequence_numbers_.insert(keyframe_sequence_numbers_.cend(), seq_num);
  }
  if (!latest_received_sequence_number_) {
    latest_received_sequence_number_ = seq_num;
    return {false, false};
  }
  // Purge old keyframes
  const auto newest_received_sequence_number = latest_sequence_number(*latest_received_sequence_number_, seq_num);
  drop_packets_from_set(keyframe_sequence_numbers_,
      newest_received_sequence_number - constants::max_packet_age_to_nack);
  const auto should_ask_pli = !update_nack_list(seq_num);
  latest_received_sequence_number_ = newest_received_sequence_number;
  return {!missing_sequence_numbers_.empty() && is_time_to_send(clock_->now()), should_ask_pli};
}

bool RtcpNewNackGenerator::missing_too_old_packet(const uint16_t latest_sequence_number) {
  if (missing_sequence_numbers_.empty()) {
    return false;
  }
  const uint16_t age_of_oldest_missing_packet = latest_sequence_number - *missing_sequence_numbers_.cbegin();
  return age_of_oldest_missing_packet > constants::max_packet_age_to_nack;
}

bool RtcpNewNackGenerator::handle_too_large_nack_list() {
  ELOG_DEBUG("NACK list has grown too large");
  bool key_frame_found = false;
  while (too_large_nack_list()) {
    key_frame_found = recycle_frames_until_key_frame();
  }
  return key_frame_found;
}

bool RtcpNewNackGenerator::handle_too_old_packets(const uint16_t latest_sequence_number) {
  ELOG_DEBUG("NACK list contains too old sequence numbers");
  bool key_frame_found = false;
  while (missing_too_old_packet(latest_sequence_number)) {
    key_frame_found = recycle_frames_until_key_frame();
  }
  return key_frame_found;
}

bool RtcpNewNackGenerator::recycle_frames_until_key_frame() {
  // should use a frame buffer strategy, for now approximate by seeking the starting packet of the next key frame
  std::pair<bool, int16_t> key_frame = extract_oldest_keyframe();
  if (key_frame.first) {
    drop_packets_from_set(missing_sequence_numbers_, key_frame.second);
    ELOG_DEBUG("recycling frames... found keyframe at seq: %u", key_frame.second);
  } else {
    ELOG_DEBUG("recycling frames... dropping all");
    missing_sequence_numbers_.clear();
  }
  return key_frame.first;
}

bool RtcpNewNackGenerator::update_nack_list(const uint16_t seq) {
  if (is_newer_sequence_number(seq, *latest_received_sequence_number_)) {
    for (uint16_t i = *latest_received_sequence_number_ + 1; is_newer_sequence_number(seq, i); ++i) {
      missing_sequence_numbers_.insert(missing_sequence_numbers_.cend(), i);
      ELOG_DEBUG("added sequence number to NACK list: %u", i);
    }
    if (too_large_nack_list() && !handle_too_large_nack_list()) {
      return false;
    }
    if (missing_too_old_packet(seq) && !handle_too_old_packets(seq)) {
      return false;
    }
  } else {
    missing_sequence_numbers_.erase(seq);
    ELOG_DEBUG("recovered packet: %u", seq);
  }
  return true;
}

std::pair<bool, uint16_t> RtcpNewNackGenerator::extract_oldest_keyframe() {
  if (keyframe_sequence_numbers_.empty()) {
    return {false, 0};
  }
  auto seq = *keyframe_sequence_numbers_.cbegin();
  keyframe_sequence_numbers_.erase(keyframe_sequence_numbers_.cbegin());
  return {true, seq};
}

bool RtcpNewNackGenerator::addNackPacketToRr(std::shared_ptr<DataPacket>& rr_packet) {
  const auto now = clock_->now();
  if (!is_time_to_send(now)) {
    return false;
  }
  rtcp_send_time_ = now;
  const auto nack_blocks = build_nack_blocks(missing_sequence_numbers_);
  attach_nack_to_rr(ssrc_, rr_packet, nack_blocks);
  return true;
}

std::chrono::milliseconds RtcpNewNackGenerator::rtcp_min_interval() const {
  return std::chrono::milliseconds(nack_interval);
}

bool RtcpNewNackGenerator::is_time_to_send(clock::time_point tp) const {
  return (rtcp_send_time_ + rtcp_min_interval() <= tp);
}

namespace {
std::vector<NackBlock> build_nack_blocks(const Seq_number_set& set) {
  std::vector<NackBlock> nack_blocks;
  for (auto it = set.cbegin(); it != set.cend(); ++it) {
    if (nack_blocks.size() >= max_nack_blocks) {
      break;
    }
    const uint16_t pid = *it;
    ELOG_TRACE2(RtcpNewNackGenerator::logger, "added NACK PID, seq_num %u", pid);
    uint16_t blp = 0;
    for (++it; it != set.cend(); ++it) {
      uint16_t distance = *it - pid;
      if (distance > 16) {
        break;
      }
      ELOG_TRACE2(RtcpNewNackGenerator::logger, "added Nack to BLP, seq_num: %u", *it);
      blp |= (1 << (distance - 1));
    }
    --it;
    NackBlock block;
    block.setNackPid(pid);
    block.setNackBlp(blp);
    nack_blocks.push_back(block);
  }
  return nack_blocks;
}

void attach_nack_to_rr(const uint32_t ssrc,
    std::shared_ptr<DataPacket>& rr_packet,  // NOLINT
    const std::vector<NackBlock>& nack_blocks) {
  char* buffer = rr_packet->data;
  buffer += rr_packet->length;

  RtcpHeader nack_packet;
  nack_packet.setPacketType(RTCP_RTP_Feedback_PT);
  nack_packet.setBlockCount(1);
  nack_packet.setSSRC(ssrc);
  nack_packet.setSourceSSRC(ssrc);
  nack_packet.setLength(kNackCommonHeaderLengthRtcp + nack_blocks.size());
  memcpy(buffer, reinterpret_cast<char *>(&nack_packet), kNackCommonHeaderLengthBytes);
  buffer += kNackCommonHeaderLengthBytes;

  memcpy(buffer, nack_blocks.data(), nack_blocks.size()*4);
  int nack_length = (nack_packet.getLength()+1)*4;

  rr_packet->length += nack_length;
}
}  // namespace
}  // namespace erizo
