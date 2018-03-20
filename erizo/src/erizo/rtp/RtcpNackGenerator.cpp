#include <algorithm>
#include "rtp/RtcpNackGenerator.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtcpNackGenerator, "rtp.RtcpNackGenerator");

static const int kMaxRetransmits = 2;
static const int kMaxNacks = 150;
static const int kMinNackDelayMs = 20;
static const int kNackCommonHeaderLengthRtcp = kNackCommonHeaderLengthBytes/4 - 1;

RtcpNackGenerator::RtcpNackGenerator(uint32_t ssrc, std::shared_ptr<Clock> the_clock) :
  initialized_{false}, highest_seq_num_{0}, ssrc_{ssrc}, clock_{the_clock} {}

bool RtcpNackGenerator::handleRtpPacket(std::shared_ptr<DataPacket> packet) {
  if (packet->type != VIDEO_PACKET) {
    return false;
  }
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  uint16_t seq_num = head->getSeqNumber();
  if (head->getSSRC() != ssrc_) {
    ELOG_DEBUG("message: handleRtpPacket Unknown SSRC, ssrc: %u", head->getSSRC());
    return false;
  }
  if (!initialized_) {
    highest_seq_num_ = seq_num;
    initialized_ = true;
    return false;
  }
  if (seq_num == highest_seq_num_) {
    return false;
  }
  // TODO(pedro) Consider clearing the nack list if this is a keyframe
  if (RtpUtils::sequenceNumberLessThan(seq_num, highest_seq_num_)) {
    ELOG_DEBUG("message: packet out of order, ssrc: %u, seq_num: %u, highest_seq_num: %u",
        seq_num, highest_seq_num_, ssrc_);
    //  Look for it in nack list, remove it if its there
    auto nack_info = std::find_if(nack_info_list_.begin(), nack_info_list_.end(),
        [seq_num](NackInfo& current_nack) {
        return (current_nack.seq_num == seq_num);
        });
    if (nack_info != nack_info_list_.end()) {
      ELOG_DEBUG("message: Recovered Packet %u", seq_num);
      nack_info_list_.erase(nack_info);
    }
    return false;
  }
  bool available_nacks = addNacks(seq_num);
  highest_seq_num_ = seq_num;
  return available_nacks;
}

bool RtcpNackGenerator::addNacks(uint16_t seq_num) {
  for (uint16_t current_seq_num = highest_seq_num_ + 1; current_seq_num != seq_num; current_seq_num++) {
    ELOG_DEBUG("message: Inserting a new Nack in list, ssrc: %u, seq_num: %u", ssrc_, current_seq_num);
    nack_info_list_.push_back(NackInfo{current_seq_num});
  }
  while (nack_info_list_.size() > kMaxNacks) {
     nack_info_list_.erase(nack_info_list_.end() - 1);
  }
  return !nack_info_list_.empty();
}

bool RtcpNackGenerator::addNackPacketToRr(std::shared_ptr<DataPacket> rr_packet) {
  // Goes through the list adds blocks of 16 in compound packets (adds more PID/BLP blocks) max is 10 blocks
  // Only does it if it's time (> 100 ms since last NACK)
  std::vector <NackBlock> nack_vector;
  ELOG_DEBUG("message: Adding nacks to RR, nack_info_list_.size(): %lu", nack_info_list_.size());
  uint64_t now_ms = ClockUtils::timePointToMs(clock_->now());
  for (uint16_t index = 0; index < nack_info_list_.size(); index++) {
    NackInfo& base_nack_info = nack_info_list_[index];
    if (!isTimeToRetransmit(base_nack_info, now_ms)) {
      ELOG_DEBUG("It's not time to retransmit %lu, now %lu, diff %lu", base_nack_info.sent_time, now_ms,
          now_ms - base_nack_info.sent_time);
      continue;
    }
    if (base_nack_info.retransmits >= kMaxRetransmits) {
      ELOG_DEBUG("message: Removing Nack in list too many retransmits, ssrc: %u, seq_num: %u",
          ssrc_, base_nack_info.seq_num);
        nack_info_list_.erase(nack_info_list_.begin() + index);
        index--;  // Items are moved in the list so the next element has the current index
        continue;
    }
    ELOG_DEBUG("message: PID, seq_num %u", base_nack_info.seq_num);
    uint16_t pid = base_nack_info.seq_num;
    uint16_t blp = 0;
    base_nack_info.sent_time = now_ms;
    base_nack_info.retransmits++;
    while (index + 1u < nack_info_list_.size()) {
      NackInfo& blp_nack_info = nack_info_list_[index + 1];
      uint16_t distance = blp_nack_info.seq_num - pid -1;
      if (distance <= 15) {
        if (!isTimeToRetransmit(blp_nack_info, now_ms)) {
          index++;
          continue;
        }
        if (blp_nack_info.retransmits >= kMaxRetransmits) {
          ELOG_DEBUG("message: Removing Nack in list too many retransmits, ssrc: %u, seq_num: %u",
              ssrc_, blp_nack_info.seq_num);
          nack_info_list_.erase(nack_info_list_.begin() + index + 1);
          continue;
        }
        ELOG_DEBUG("message: Adding Nack to BLP, seq_num: %u", blp_nack_info.seq_num);
        blp |= (1 << distance);
        blp_nack_info.sent_time = now_ms;
        blp_nack_info.retransmits++;
        index++;
      } else {
        break;
      }
    }
    NackBlock block;
    block.setNackPid(pid);
    block.setNackBlp(blp);
    nack_vector.push_back(block);
  }
  if (nack_vector.size() == 0) {
    return false;
  }

  char* buffer = rr_packet->data;
  buffer += rr_packet->length;

  RtcpHeader nack_packet;
  nack_packet.setPacketType(RTCP_RTP_Feedback_PT);
  nack_packet.setBlockCount(1);
  nack_packet.setSSRC(ssrc_);
  nack_packet.setSourceSSRC(ssrc_);
  nack_packet.setLength(kNackCommonHeaderLengthRtcp + nack_vector.size());
  memcpy(buffer, reinterpret_cast<char *>(&nack_packet), kNackCommonHeaderLengthBytes);
  buffer += kNackCommonHeaderLengthBytes;

  memcpy(buffer, &nack_vector[0], nack_vector.size()*4);
  int nack_length = (nack_packet.getLength()+1)*4;

  rr_packet->length += nack_length;
  return true;
}

bool RtcpNackGenerator::isTimeToRetransmit(const NackInfo& nack_info, uint64_t current_time_ms) {
  return (nack_info.sent_time == 0 || (current_time_ms - nack_info.sent_time) > kMinNackDelayMs);
}

}  // namespace erizo
