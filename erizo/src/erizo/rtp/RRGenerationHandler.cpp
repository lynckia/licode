#include "rtp/RRGenerationHandler.h"
#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(RRGenerationHandler, "rtp.RRGenerationHandler");

RRGenerationHandler::RRGenerationHandler(bool use_timing)
  : connection_{nullptr}, enabled_{true}, initialized_{false}, use_timing_{use_timing},
    generator_{random_device_()} {}

RRGenerationHandler::RRGenerationHandler(const RRGenerationHandler&& handler) :  // NOLINT
    connection_{handler.connection_},
    enabled_{handler.enabled_},
    initialized_{handler.initialized_},
    use_timing_{handler.use_timing_},
    rr_info_map_{std::move(handler.rr_info_map_)} {}

void RRGenerationHandler::enable() {
  enabled_ = true;
}

void RRGenerationHandler::disable() {
  enabled_ = false;
}

bool RRGenerationHandler::isRetransmitOfOldPacket(std::shared_ptr<dataPacket> packet,
    std::shared_ptr<RRPackets> rr_info) {
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  if (!rtpSequenceLessThan(head->getSeqNumber(), rr_info->max_seq) || rr_info->jitter.jitter == 0) {
    return false;
  }
  int64_t time_diff_ms = static_cast<uint32_t>(packet->received_time_ms) - rr_info->last_recv_ts;
  int64_t timestamp_diff = static_cast<int32_t>(head->getTimestamp() - rr_info->last_rtp_ts);
  uint16_t clock_rate = rr_info->type == VIDEO_PACKET ? getVideoClockRate(head->getPayloadType()) :
    getAudioClockRate(head->getPayloadType());
  int64_t rtp_time_stamp_diff_ms = timestamp_diff / clock_rate;
  int64_t max_delay_ms = ((2 * rr_info->jitter.jitter) /  clock_rate);
  return time_diff_ms > rtp_time_stamp_diff_ms + max_delay_ms;
}

bool RRGenerationHandler::rtpSequenceLessThan(uint16_t x, uint16_t y) {
  int diff = y - x;
  if (diff > 0) {
    return (diff < 0x8000);
  } else if (diff < 0) {
    return (diff < -0x8000);
  } else {
    return false;
  }
}

// TODO(kekkokk) Consider add more payload types
int RRGenerationHandler::getAudioClockRate(uint8_t payload_type) {
  if (payload_type == OPUS_48000_PT) {
    return 48;
  } else {
    return 8;
  }
}

// TODO(kekkokk) ATM we only use 90Khz video type so always return 90
int RRGenerationHandler::getVideoClockRate(uint8_t payload_type) {
  return 90;
}

void RRGenerationHandler::handleRtpPacket(std::shared_ptr<dataPacket> packet) {
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  auto rr_packet_pair = rr_info_map_.find(head->getSSRC());
  if (rr_packet_pair == rr_info_map_.end()) {
    ELOG_DEBUG("%s message: handleRtpPacket ssrc not found, ssrc: %u", connection_->toLog(), head->getSSRC());
    return;
  }
  std::shared_ptr<RRPackets> selected_packet_info = rr_packet_pair->second;
  uint16_t seq_num = head->getSeqNumber();
  selected_packet_info->packets_received++;
  if (selected_packet_info->base_seq == -1) {
    selected_packet_info->ssrc = head->getSSRC();
    selected_packet_info->base_seq = head->getSeqNumber();
  }
  if (selected_packet_info->max_seq == -1) {
    selected_packet_info->max_seq = seq_num;
  } else if (!rtpSequenceLessThan(seq_num, selected_packet_info->max_seq)) {
    if (seq_num < selected_packet_info->max_seq) {
      selected_packet_info->cycle++;
    }
    selected_packet_info->max_seq = seq_num;
  }
  selected_packet_info->extended_seq = (selected_packet_info->cycle << 16) | selected_packet_info->max_seq;

  uint16_t clock_rate = selected_packet_info->type == VIDEO_PACKET ? getVideoClockRate(head->getPayloadType()) :
    getAudioClockRate(head->getPayloadType());
  if (head->getTimestamp() != selected_packet_info->last_rtp_ts &&
      !isRetransmitOfOldPacket(packet, selected_packet_info)) {
    int transit_time = static_cast<int>((packet->received_time_ms * clock_rate) - head->getTimestamp());
    int delta = abs(transit_time - selected_packet_info->jitter.transit_time);
    if (selected_packet_info->jitter.transit_time != 0 && delta < MAX_DELAY) {
      selected_packet_info->jitter.jitter +=
        (1. / 16.) * (static_cast<double>(delta) - selected_packet_info->jitter.jitter);
    }
    selected_packet_info->jitter.transit_time = transit_time;
  }
  selected_packet_info->last_rtp_ts = head->getTimestamp();
  selected_packet_info->last_recv_ts = static_cast<uint32_t>(packet->received_time_ms);
  uint64_t now = ClockUtils::timePointToMs(clock::now());
  if (selected_packet_info->next_packet_ms == 0) {  // Schedule the first packet
    uint16_t selected_interval = selectInterval(selected_packet_info);
    selected_packet_info->next_packet_ms = now + selected_interval;
    return;
  }

  if (now >= selected_packet_info->next_packet_ms) {
    sendRR(selected_packet_info);
  }
}

void RRGenerationHandler::sendRR(std::shared_ptr<RRPackets> selected_packet_info) {
  if (selected_packet_info->ssrc != 0) {
    uint64_t now = ClockUtils::timePointToMs(clock::now());
    uint64_t delay_since_last_sr = selected_packet_info->last_sr_ts == 0 ?
      0 : (now - selected_packet_info->last_sr_ts) * 65536 / 1000;
    RtcpHeader rtcp_head;
    rtcp_head.setPacketType(RTCP_Receiver_PT);
    rtcp_head.setSSRC(selected_packet_info->ssrc);
    rtcp_head.setSourceSSRC(selected_packet_info->ssrc);
    rtcp_head.setHighestSeqnum(selected_packet_info->extended_seq);
    rtcp_head.setSeqnumCycles(selected_packet_info->cycle);
    rtcp_head.setLostPackets(selected_packet_info->lost);
    rtcp_head.setFractionLost(selected_packet_info->frac_lost);
    rtcp_head.setJitter(static_cast<uint32_t>(selected_packet_info->jitter.jitter));
    rtcp_head.setDelaySinceLastSr(static_cast<uint32_t>(delay_since_last_sr));
    rtcp_head.setLastSr(selected_packet_info->last_sr_mid_ntp);
    rtcp_head.setLength(7);
    rtcp_head.setBlockCount(1);
    int length = (rtcp_head.getLength() + 1) * 4;

    memcpy(packet_, reinterpret_cast<uint8_t*>(&rtcp_head), length);
    getContext()->fireWrite(std::make_shared<dataPacket>(0, reinterpret_cast<char*>(&packet_), length, OTHER_PACKET));
    selected_packet_info->last_rr_ts = now;

    ELOG_DEBUG("%s, message: Sending RR, ssrc: %u, type: %u lost: %u, frac: %u, cycle: %u, highseq: %u, jitter: %u, "
        "dlsr: %u, lsr: %u", connection_->toLog(), selected_packet_info->ssrc, selected_packet_info->type,
        rtcp_head.getLostPackets(), rtcp_head.getFractionLost(), rtcp_head.getSeqnumCycles(),
        rtcp_head.getHighestSeqnum(), rtcp_head.getJitter(), rtcp_head.getDelaySinceLastSr(),
        rtcp_head.getLastSr());

    uint16_t selected_interval = selectInterval(selected_packet_info);
    selected_packet_info->next_packet_ms = now + getRandomValue(0.5 * selected_interval, 1.5 * selected_interval);
  }
}


void RRGenerationHandler::handleSR(std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  auto rr_packet_pair = rr_info_map_.find(chead->getSSRC());
  if (rr_packet_pair == rr_info_map_.end()) {
    ELOG_DEBUG("%s message: handleRtpPacket ssrc not found, ssrc: %u", connection_->toLog(), chead->getSSRC());
    return;
  }
  std::shared_ptr<RRPackets> selected_packet_info = rr_packet_pair->second;

  selected_packet_info->last_sr_mid_ntp = chead->get32MiddleNtp();
  selected_packet_info->last_sr_ts = packet->received_time_ms;
  uint32_t expected = selected_packet_info->extended_seq - selected_packet_info->base_seq + 1;
  selected_packet_info->lost = expected - selected_packet_info->packets_received;

  uint8_t fraction = 0;
  uint32_t expected_interval = expected - selected_packet_info->expected_prior;
  selected_packet_info->expected_prior = expected;
  uint32_t received_interval = selected_packet_info->packets_received - selected_packet_info->received_prior;

  selected_packet_info->received_prior = selected_packet_info->packets_received;
  uint32_t lost_interval = expected_interval - received_interval;
  if (expected_interval != 0 && lost_interval > 0) {
    fraction = (lost_interval << 8) / expected_interval;
  }

  selected_packet_info->frac_lost = fraction;
  if (!use_timing_) {
    sendRR(selected_packet_info);
  }
}

void RRGenerationHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && enabled_) {
    handleRtpPacket(packet);
  } else if (chead->packettype == RTCP_Sender_PT && enabled_) {
    handleSR(packet);
  }
  ctx->fireRead(packet);
}

void RRGenerationHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  ctx->fireWrite(packet);
}

void RRGenerationHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  connection_ = pipeline->getService<WebRtcConnection>().get();
  if (!connection_) {
    return;
  }
  std::vector<uint32_t> video_ssrc_list = connection_->getVideoSourceSSRCList();
  std::for_each(video_ssrc_list.begin(), video_ssrc_list.end(), [this] (uint32_t video_ssrc){
      if (video_ssrc != 0) {
        auto video_packets = std::make_shared<RRPackets>();
        video_packets->ssrc = video_ssrc;
        video_packets->type = VIDEO_PACKET;
        rr_info_map_[video_ssrc] = video_packets;
        ELOG_DEBUG("%s, message: Initialized video, ssrc: %u", connection_->toLog(), video_ssrc);
        initialized_ = true;
      }
  });
  uint32_t audio_ssrc = connection_->getAudioSourceSSRC();
  if (audio_ssrc != 0) {
    auto audio_packets = std::make_shared<RRPackets>();
    audio_packets->ssrc = audio_ssrc;
    audio_packets->type = AUDIO_PACKET;
    rr_info_map_[audio_ssrc] = audio_packets;
    initialized_ = true;
    ELOG_DEBUG("%s, message: Initialized audio, ssrc: %u", connection_->toLog(), audio_ssrc);
  }
}

uint16_t RRGenerationHandler::selectInterval(std::shared_ptr<RRPackets> packet_info) {
    return (packet_info->type == VIDEO_PACKET ? RTCP_VIDEO_INTERVAL : RTCP_AUDIO_INTERVAL);
}

uint16_t RRGenerationHandler::getRandomValue(uint16_t min, uint16_t max) {
  std::uniform_int_distribution<> distr(min, max);
  return std::round(distr(generator_));
}

}  // namespace erizo
