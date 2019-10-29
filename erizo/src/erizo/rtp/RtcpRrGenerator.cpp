#include "rtp/RtcpRrGenerator.h"
#include "lib/ClockUtils.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtcpRrGenerator, "rtp.RtcpRrGenerator");

RtcpRrGenerator::RtcpRrGenerator(uint32_t ssrc, packetType type, std::shared_ptr<Clock> the_clock)
  : rr_info_{RrPacketInfo(ssrc)}, ssrc_{ssrc}, type_{type},
   random_generator_{random_device_()}, clock_{the_clock} {}

RtcpRrGenerator::RtcpRrGenerator(const RtcpRrGenerator&& generator) :  // NOLINT
    rr_info_{std::move(generator.rr_info_)},
    ssrc_{generator.ssrc_},
    type_{generator.type_} {}

bool RtcpRrGenerator::isRetransmitOfOldPacket(std::shared_ptr<DataPacket> packet) {
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  if (!RtpUtils::sequenceNumberLessThan(head->getSeqNumber(), rr_info_.max_seq) || rr_info_.jitter.jitter == 0) {
    return false;
  }
  int64_t time_diff_ms = static_cast<uint32_t>(packet->received_time_ms) - rr_info_.last_recv_ts;
  int64_t timestamp_diff = static_cast<int32_t>(head->getTimestamp() - rr_info_.last_rtp_ts);
  uint16_t clock_rate = type_ == VIDEO_PACKET ? getVideoClockRate(head->getPayloadType()) :
    getAudioClockRate(head->getPayloadType());
  int64_t rtp_time_stamp_diff_ms = timestamp_diff / clock_rate;
  int64_t max_delay_ms = ((2 * rr_info_.jitter.jitter) /  clock_rate);
  return time_diff_ms > rtp_time_stamp_diff_ms + max_delay_ms;
}

// TODO(kekkokk) Consider add more payload types
int RtcpRrGenerator::getAudioClockRate(uint8_t payload_type) {
  if (payload_type == OPUS_48000_PT) {
    return 48;
  } else {
    return 8;
  }
}

// TODO(kekkokk) ATM we only use 90Khz video type so always return 90
int RtcpRrGenerator::getVideoClockRate(uint8_t payload_type) {
  return 90;
}

bool RtcpRrGenerator::handleRtpPacket(std::shared_ptr<DataPacket> packet) {
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  if (ssrc_ != head->getSSRC()) {
    ELOG_DEBUG("message: handleRtpPacket ssrc not found, ssrc: %u", head->getSSRC());
    return false;
  }
  uint16_t seq_num = head->getSeqNumber();
  rr_info_.packets_received++;
  if (rr_info_.base_seq == -1) {
    rr_info_.base_seq = head->getSeqNumber();
  }
  if (rr_info_.max_seq == -1) {
    rr_info_.max_seq = seq_num;
  } else if (!RtpUtils::sequenceNumberLessThan(seq_num, rr_info_.max_seq)) {
    if (seq_num < rr_info_.max_seq) {
      rr_info_.cycle++;
    }
    rr_info_.max_seq = seq_num;
  }
  rr_info_.extended_seq = (rr_info_.cycle << 16) | rr_info_.max_seq;

  uint16_t clock_rate = type_ == VIDEO_PACKET ? getVideoClockRate(head->getPayloadType()) :
    getAudioClockRate(head->getPayloadType());
  if (head->getTimestamp() != rr_info_.last_rtp_ts &&
      !isRetransmitOfOldPacket(packet)) {
    int transit_time = static_cast<int>((packet->received_time_ms * clock_rate) - head->getTimestamp());
    int delta = abs(transit_time - rr_info_.jitter.transit_time);
    if (rr_info_.jitter.transit_time != 0 && delta < MAX_DELAY) {
      rr_info_.jitter.jitter +=
        (1. / 16.) * (static_cast<double>(delta) - rr_info_.jitter.jitter);
    }
    rr_info_.jitter.transit_time = transit_time;
  }
  rr_info_.last_rtp_ts = head->getTimestamp();
  rr_info_.last_recv_ts = static_cast<uint32_t>(packet->received_time_ms);
  uint64_t now = ClockUtils::timePointToMs(clock_->now());
  if (rr_info_.next_packet_ms == 0) {  // Schedule the first packet
    uint16_t selected_interval = selectInterval();
    rr_info_.next_packet_ms = now + selected_interval;
    return false;
  }

  if (now >= rr_info_.next_packet_ms) {
    ELOG_DEBUG("message: should send packet, ssrc: %u", ssrc_);
    return true;
  }
  return false;
}

std::shared_ptr<DataPacket> RtcpRrGenerator::generateReceiverReport() {
  uint64_t now = ClockUtils::timePointToMs(clock_->now());
  uint64_t delay_since_last_sr = rr_info_.last_sr_ts == 0 ?
    0 : (now - rr_info_.last_sr_ts) * 65536 / 1000;
  uint32_t expected = rr_info_.extended_seq - rr_info_.base_seq + 1;
  // TODO(pedro): This is the previous way to calculate packet loss
  // it accounts for packets recovered with retransmissions and
  // Chrome does not seem to like that
  /* if (expected < rr_info_.packets_received) { */
  /*   rr_info_.lost = 0; */
  /* } else { */
  /*   rr_info_.lost = expected - rr_info_.packets_received; */
  /* } */


  uint8_t fraction = 0;
  uint32_t expected_interval = expected - rr_info_.expected_prior;
  rr_info_.expected_prior = expected;
  uint32_t received_interval = rr_info_.packets_received - rr_info_.received_prior;

  rr_info_.received_prior = rr_info_.packets_received;
  int64_t lost_interval = static_cast<int64_t>(expected_interval) - received_interval;
  if (expected_interval != 0 && lost_interval > 0) {
    fraction = (static_cast<uint32_t>(lost_interval) << 8) / expected_interval;
  }

  // TODO(pedro): We're getting closer to packet loss without retransmissions by ignoring negative
  // lost in the interval. This is not perfect but will provide a more "monotonically increasing" behavior
  if (lost_interval > 0) {
    rr_info_.lost += lost_interval;
  }

  rr_info_.frac_lost = fraction;
  RtcpHeader rtcp_head;
  rtcp_head.setPacketType(RTCP_Receiver_PT);
  rtcp_head.setSSRC(ssrc_);
  rtcp_head.setSourceSSRC(ssrc_);
  rtcp_head.setHighestSeqnum(rr_info_.extended_seq);
  rtcp_head.setSeqnumCycles(rr_info_.cycle);
  rtcp_head.setLostPackets(rr_info_.lost);
  rtcp_head.setFractionLost(rr_info_.frac_lost);
  rtcp_head.setJitter(static_cast<uint32_t>(rr_info_.jitter.jitter));
  rtcp_head.setDelaySinceLastSr(static_cast<uint32_t>(delay_since_last_sr));
  rtcp_head.setLastSr(rr_info_.last_sr_mid_ntp);
  rtcp_head.setLength(7);
  rtcp_head.setBlockCount(1);
  int length = (rtcp_head.getLength() + 1) * 4;

  memcpy(packet_, reinterpret_cast<uint8_t*>(&rtcp_head), length);
  rr_info_.last_rr_ts = now;

  ELOG_DEBUG("message: Sending RR, ssrc: %u, type: %u lost: %u, frac: %u, cycle: %u, highseq: %u, jitter: %u, "
      "dlsr: %u, lsr: %u", ssrc_, type_,
      rtcp_head.getLostPackets(), rtcp_head.getFractionLost(), rtcp_head.getSeqnumCycles(),
      rtcp_head.getHighestSeqnum(), rtcp_head.getJitter(), rtcp_head.getDelaySinceLastSr(),
      rtcp_head.getLastSr());

  uint16_t selected_interval = selectInterval();
  rr_info_.next_packet_ms = now + getRandomValue(0.5 * selected_interval, 1.5 * selected_interval);
  rr_info_.last_packet_ms = now;
  return (std::make_shared<DataPacket>(0, reinterpret_cast<char*>(&packet_), length, type_));
}


void RtcpRrGenerator::handleSr(std::shared_ptr<DataPacket> packet) {
  RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (ssrc_ != chead->getSSRC()) {
    ELOG_DEBUG("message: handleRtpPacket ssrc not found, ssrc: %u", chead->getSSRC());
    return;
  }

  rr_info_.last_sr_mid_ntp = chead->get32MiddleNtp();
  rr_info_.last_sr_ts = packet->received_time_ms;
}

uint16_t RtcpRrGenerator::selectInterval() {
    return (type_ == VIDEO_PACKET ? RTCP_VIDEO_INTERVAL : RTCP_AUDIO_INTERVAL);
}

uint16_t RtcpRrGenerator::getRandomValue(uint16_t min, uint16_t max) {
  std::uniform_int_distribution<> distr(min, max);
  return std::round(distr(random_generator_));
}

}  // namespace erizo
