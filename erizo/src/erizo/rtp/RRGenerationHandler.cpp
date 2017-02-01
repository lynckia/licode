#include "rtp/RRGenerationHandler.h"
#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(RRGenerationHandler, "rtp.RRGenerationHandler");

RRGenerationHandler::RRGenerationHandler(WebRtcConnection *connection) :
    temp_ctx_{nullptr}, enabled_{true} {}


void RRGenerationHandler::enable() {
  enabled_ = true;
}

void RRGenerationHandler::disable() {
  enabled_ = false;
}

bool RRGenerationHandler::isRetransmitOfOldPacket(std::shared_ptr<dataPacket> packet) {
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  if (packet->type == VIDEO_PACKET) {
    if (!rtpSequenceLessThan(head->getSeqNumber(), video_rr_.max_seq) || jitter_video_.jitter == 0) {
      return false;
    }
    int64_t time_diff_ms = static_cast<uint32_t>(packet->received_time_ms) - video_rr_.last_recv_ts;
    int64_t timestamp_diff = static_cast<int32_t>(head->getTimestamp() - video_rr_.last_rtp_ts);
    int64_t rtp_time_stamp_diff_ms = timestamp_diff / getVideoClockRate(head->getPayloadType());
    int64_t max_delay_ms = ((2 * jitter_video_.jitter) /  getVideoClockRate(head->getPayloadType()));
    return time_diff_ms > rtp_time_stamp_diff_ms + max_delay_ms;
  } else if (packet->type == AUDIO_PACKET) {
    if (!rtpSequenceLessThan(head->getSeqNumber(), audio_rr_.max_seq) || jitter_audio_.jitter == 0) {
      return false;
    }
    int64_t time_diff_ms = static_cast<uint32_t>(packet->received_time_ms) - audio_rr_.last_recv_ts;
    int64_t timestamp_diff = static_cast<int32_t>(head->getTimestamp() - audio_rr_.last_rtp_ts);
    int64_t rtp_time_stamp_diff_ms = timestamp_diff / getAudioClockRate(head->getPayloadType());
    int64_t max_delay_ms = ((2 * jitter_audio_.jitter) /  getAudioClockRate(head->getPayloadType()));
    return time_diff_ms > rtp_time_stamp_diff_ms + max_delay_ms;
  }
  return true;
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
  uint16_t seq_num = head->getSeqNumber();

  switch (packet->type) {
  case VIDEO_PACKET: {
    video_rr_.p_received++;
    if (video_rr_.base_seq == -1) {
      video_rr_.ssrc = head->getSSRC();
      video_rr_.base_seq = head->getSeqNumber();
    }
    if (video_rr_.max_seq == -1) {
      video_rr_.max_seq = seq_num;
    } else if (!rtpSequenceLessThan(seq_num, video_rr_.max_seq)) {
      if (seq_num < video_rr_.max_seq) {
        video_rr_.cycle++;
      }
      video_rr_.max_seq = seq_num;
    }
    video_rr_.extended_seq = (video_rr_.cycle << 16) | video_rr_.max_seq;
    int clock_rate = getVideoClockRate(head->getPayloadType());
    if (head->getTimestamp() != video_rr_.last_rtp_ts && !isRetransmitOfOldPacket(packet)) {
      int transit_time = static_cast<int>((packet->received_time_ms * clock_rate) - head->getTimestamp());
      int delta = abs(transit_time - jitter_video_.transit_time);
      if (jitter_video_.transit_time != 0 && delta < MAX_DELAY) {
        jitter_video_.jitter += (1. / 16.) * (static_cast<double>(delta) - jitter_video_.jitter);
      }
      jitter_video_.transit_time = transit_time;
    }
    video_rr_.last_rtp_ts = head->getTimestamp();
    video_rr_.last_recv_ts = static_cast<uint32_t>(packet->received_time_ms);
    break;
  }
  case AUDIO_PACKET: {
    audio_rr_.p_received++;
    if (audio_rr_.base_seq == -1) {
      audio_rr_.ssrc = head->getSSRC();
      audio_rr_.base_seq = head->getSeqNumber();
    }
    if (audio_rr_.max_seq == -1) {
      audio_rr_.max_seq = seq_num;
    } else if (!rtpSequenceLessThan(seq_num, audio_rr_.max_seq)) {
      if (seq_num < audio_rr_.max_seq) {
        audio_rr_.cycle++;
      }
      audio_rr_.max_seq = seq_num;
    }
    audio_rr_.extended_seq = (audio_rr_.cycle << 16) | audio_rr_.max_seq;
    int clock_rate = getAudioClockRate(head->getPayloadType());
    if (head->getTimestamp() != audio_rr_.last_rtp_ts && !isRetransmitOfOldPacket(packet)) {
      int transit_time = static_cast<int>((packet->received_time_ms * clock_rate) - head->getTimestamp());
      int delta = abs(transit_time - jitter_audio_.transit_time);
      if (jitter_audio_.transit_time != 0 && delta < MAX_DELAY) {
        jitter_audio_.jitter += (1. / 16.) * (static_cast<double>(delta) - jitter_audio_.jitter);
      }
      jitter_audio_.transit_time = transit_time;
    }
    audio_rr_.last_rtp_ts = head->getTimestamp();
    audio_rr_.last_recv_ts = packet->received_time_ms;
    break;
  }
  default:
    break;
  }
}

void RRGenerationHandler::sendVideoRR() {
  int64_t now = ClockUtils::timePointToMs(clock::now());

  if (video_rr_.ssrc != 0) {
    uint64_t delay_since_last_sr = video_rr_.last_sr_ts == 0 ? 0 : (now - video_rr_.last_sr_ts) * 65536 / 1000;
    RtcpHeader rtcp_head;
    rtcp_head.setPacketType(RTCP_Receiver_PT);
    rtcp_head.setSSRC(video_rr_.ssrc);
    rtcp_head.setSourceSSRC(video_rr_.ssrc);
    rtcp_head.setHighestSeqnum(video_rr_.extended_seq);
    rtcp_head.setSeqnumCycles(video_rr_.cycle);
    rtcp_head.setLostPackets(video_rr_.lost);
    rtcp_head.setFractionLost(video_rr_.frac_lost);
    rtcp_head.setJitter(static_cast<uint32_t>(jitter_video_.jitter));
    rtcp_head.setDelaySinceLastSr(static_cast<uint32_t>(delay_since_last_sr));
    rtcp_head.setLastSr(video_rr_.last_sr_mid_ntp);
    rtcp_head.setLength(7);
    rtcp_head.setBlockCount(1);
    int length = (rtcp_head.getLength() + 1) * 4;
    memcpy(packet_, reinterpret_cast<uint8_t*>(&rtcp_head), length);
    if (temp_ctx_) {
      temp_ctx_->fireWrite(std::make_shared<dataPacket>(0, reinterpret_cast<char*>(&packet_), length, OTHER_PACKET));
      video_rr_.last_rr_ts = now;
      ELOG_DEBUG("Sending video RR- lost: %u, frac: %u, cycle: %u, highseq: %u, jitter: %u, "
                "dlsr: %u, lsr: %u", rtcp_head.getLostPackets(), rtcp_head.getFractionLost(),
                rtcp_head.getSeqnumCycles(), rtcp_head.getHighestSeqnum(), rtcp_head.getJitter(),
                rtcp_head.getDelaySinceLastSr(), rtcp_head.getLastSr());
    }
  }
}

void RRGenerationHandler::sendAudioRR() {
  int64_t now = ClockUtils::timePointToMs(clock::now());

  if (audio_rr_.ssrc != 0) {
    uint32_t delay_since_last_sr = audio_rr_.last_sr_ts == 0 ? 0 : (now - audio_rr_.last_sr_ts) * 65536 / 1000;
    RtcpHeader rtcp_head;
    rtcp_head.setPacketType(RTCP_Receiver_PT);
    rtcp_head.setSSRC(audio_rr_.ssrc);
    rtcp_head.setSourceSSRC(audio_rr_.ssrc);
    rtcp_head.setHighestSeqnum(audio_rr_.extended_seq);
    rtcp_head.setSeqnumCycles(audio_rr_.cycle);
    rtcp_head.setLostPackets(audio_rr_.lost);
    rtcp_head.setFractionLost(audio_rr_.frac_lost);
    rtcp_head.setJitter(static_cast<uint32_t>(jitter_audio_.jitter));
    rtcp_head.setDelaySinceLastSr(delay_since_last_sr);
    rtcp_head.setLastSr(audio_rr_.last_sr_mid_ntp);
    rtcp_head.setLength(7);
    rtcp_head.setBlockCount(1);
    int length = (rtcp_head.getLength() + 1) * 4;
    memcpy(packet_, reinterpret_cast<uint8_t*>(&rtcp_head), length);
    if (temp_ctx_) {
      temp_ctx_->fireWrite(std::make_shared<dataPacket>(0, reinterpret_cast<char*>(&packet_), length, OTHER_PACKET));
      audio_rr_.last_rr_ts = now;
      ELOG_DEBUG("Sending audio RR - lost: %u, frac: %u, cycle: %u, highseq: %u, jitter: %u, "
                "dslr: %u, lsr: %u", rtcp_head.getLostPackets(), rtcp_head.getFractionLost(),
                rtcp_head.getSeqnumCycles(), rtcp_head.getHighestSeqnum(), rtcp_head.getJitter(),
                rtcp_head.getDelaySinceLastSr(), rtcp_head.getLastSr());
    }
  }
}

void RRGenerationHandler::handleSR(std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);

    if (chead->getSSRC() == video_rr_.ssrc) {
      video_rr_.last_sr_mid_ntp = chead->get32MiddleNtp();
      video_rr_.last_sr_ts = packet->received_time_ms;
      uint32_t expected = video_rr_.extended_seq- video_rr_.base_seq + 1;
      video_rr_.lost = expected - video_rr_.p_received;
      uint8_t fraction = 0;
      uint32_t expected_interval = expected - video_rr_.expected_prior;
      video_rr_.expected_prior = expected;
      uint32_t received_interval = video_rr_.p_received - video_rr_.received_prior;
      video_rr_.received_prior = video_rr_.p_received;
      uint32_t lost_interval = expected_interval - received_interval;
      if (expected_interval != 0 && lost_interval > 0) {
        fraction = (lost_interval << 8) / expected_interval;
      }
      video_rr_.frac_lost = fraction;
      sendVideoRR();
    }

    if (chead->getSSRC() == audio_rr_.ssrc) {
      audio_rr_.last_sr_mid_ntp = chead->get32MiddleNtp();
      audio_rr_.last_sr_ts = packet->received_time_ms;
      uint32_t expected = audio_rr_.extended_seq- audio_rr_.base_seq + 1;
      audio_rr_.lost = expected - audio_rr_.p_received;
      uint32_t fraction = 0;
      uint32_t expected_interval = expected - audio_rr_.expected_prior;
      audio_rr_.expected_prior = expected;
      uint32_t received_interval = audio_rr_.p_received - audio_rr_.received_prior;
      audio_rr_.received_prior = audio_rr_.p_received;
      uint32_t lost_interval = expected_interval - received_interval;
      if (expected_interval != 0 && lost_interval > 0) {
        fraction = (lost_interval << 8) / expected_interval;
      }
      audio_rr_.frac_lost = fraction;
      sendAudioRR();
    }
}

void RRGenerationHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  temp_ctx_ = ctx;
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
  return;
}

}  // namespace erizo
