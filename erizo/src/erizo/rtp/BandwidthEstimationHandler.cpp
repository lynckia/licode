#include "rtp/BandwidthEstimationHandler.h"

#include <vector>

#include "./WebRtcConnection.h"
#include "lib/Clock.h"
#include "lib/ClockUtils.h"

#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"

namespace erizo {

using webrtc::RemoteBitrateEstimatorSingleStream;
using webrtc::RemoteBitrateEstimatorAbsSendTime;

DEFINE_LOGGER(BandwidthEstimationHandler, "rtp.BandwidthEstimationHandler");

static const uint32_t kTimeOffsetSwitchThreshold = 30;
static const uint32_t kMinBitRateAllowed = 10;
const int kRembTimeOutThresholdMs = 2000;
const int kRembSendIntervallMs = 200;
const int kRRSendIntervalMs = 1500;
const uint32_t BandwidthEstimationHandler::kRembMinimumBitrate = 20000;

// % threshold for if we should send a new REMB asap.
const unsigned int kSendThresholdPercent = 97;

std::unique_ptr<RemoteBitrateEstimator> RemoteBitrateEstimatorPicker::pickEstimator(bool using_absolute_send_time,
                                                                                    webrtc::Clock* const clock,
                                                                                    RemoteBitrateObserver *observer) {
  std::unique_ptr<RemoteBitrateEstimator> rbe;
  if (using_absolute_send_time) {
    rbe.reset(new webrtc::RemoteBitrateEstimatorAbsSendTime(observer, clock));
  } else {
    rbe.reset(new webrtc::RemoteBitrateEstimatorSingleStream(observer, clock));
  }
  return rbe;
}

BandwidthEstimationHandler::BandwidthEstimationHandler(WebRtcConnection *connection, std::shared_ptr<Worker> worker,
    std::shared_ptr<RemoteBitrateEstimatorPicker> picker) :
  connection_{connection},
  worker_{worker},
  clock_{webrtc::Clock::GetRealTimeClock()},
  picker_{picker},
  rbe_{picker_->pickEstimator(false, clock_, this)},
  using_absolute_send_time_{false}, packets_since_absolute_send_time_{0},
  min_bitrate_bps_{kMinBitRateAllowed}, temp_ctx_{nullptr},
  bitrate_{0}, last_send_bitrate_{0}, last_remb_time_{0},
  running_{false} {
}

void BandwidthEstimationHandler::process() {
  rbe_->Process();
  std::weak_ptr<BandwidthEstimationHandler> weak_ptr = shared_from_this();
  worker_->scheduleFromNow([weak_ptr]() {
    if (auto this_ptr = weak_ptr.lock()) {
      this_ptr->process();
    }
  }, std::chrono::milliseconds(rbe_->TimeUntilNextProcess()));
}

void BandwidthEstimationHandler::updateExtensionMaps(std::array<RTPExtensions, 10> video_map,
                                                     std::array<RTPExtensions, 10> audio_map) {
  updateExtensionMap(true, video_map);
  updateExtensionMap(false, audio_map);
}

void BandwidthEstimationHandler::updateExtensionMap(bool is_video, std::array<RTPExtensions, 10> map) {
  webrtc::RTPExtensionType type;
  for (uint8_t id = 0; id < 10; id++) {
    RTPExtensions extension = map[id];
    switch (extension) {
      case UNKNOWN:
        continue;
        break;
      case SSRC_AUDIO_LEVEL:
        type = webrtc::kRtpExtensionAudioLevel;
        break;
      case ABS_SEND_TIME:
        type = webrtc::kRtpExtensionAbsoluteSendTime;
        break;
      case TOFFSET:
        type = webrtc::kRtpExtensionTransmissionTimeOffset;
        break;
      case VIDEO_ORIENTATION:
        type = webrtc::kRtpExtensionVideoRotation;
        break;
      case PLAYBACK_TIME:
        type = webrtc::kRtpExtensionPlayoutDelay;
        break;
    }
    if (is_video) {
      ext_map_video_.RegisterByType(id, type);
    } else {
      ext_map_audio_.RegisterByType(id, type);
    }
  }
}

// Implements x < y, taking into account RTP sequence number wrap
// The general idea is if there's a very large difference between
// x and y, that implies that the larger one is actually "less than"
// the smaller one.
//
// I picked 0x8000 as my "very large" threshold because it splits
// 0xffff, so it seems like a logical choice.
bool BandwidthEstimationHandler::rtpSequenceLessThan(uint16_t x, uint16_t y) {
  int diff = y - x;
  if (diff > 0) {
    return (diff < 0x8000);
  } else if (diff < 0) {
    return (diff < -0x8000);
  } else {  // diff == 0
    return false;
  }
}

void BandwidthEstimationHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  temp_ctx_ = ctx;
  if (!running_) {
    process();
    running_ = true;
  }
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (!chead->isRtcp() && packet->type == VIDEO_PACKET) {
    if (parsePacket(packet)) {
      int64_t arrival_time_ms = packet->received_time_ms;
      arrival_time_ms = clock_->TimeInMilliseconds() - (ClockUtils::timePointToMs(clock::now()) - arrival_time_ms);
      size_t payload_size = packet->length;
      pickEstimatorFromHeader();
      rbe_->IncomingPacket(arrival_time_ms, payload_size, header_);
    } else {
      ELOG_DEBUG("Packet not parsed %d", packet->type);
    }
  }

  uint32_t now = ClockUtils::timePointToMs(clock::now());
  RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
  if (!chead->isRtcp()) {
    // RTP PACKETS
    uint16_t seqNum = head->getSeqNumber();
    switch (packet->type) {
      case VIDEO_PACKET:
      // RTP VIDEO PACKET
        videoRR.ssrc = head->getSSRC();
        if (!rtpSequenceLessThan(seqNum, videoRR.last_rtp_seq_num)) {
          if (seqNum < videoRR.last_rtp_seq_num) {
            ELOG_WARN("VIDEO RTP WRAPPED");
            videoRR.cycle++;
          }
          videoRR.last_rtp_seq_num = seqNum;
        }
      break;
      case AUDIO_PACKET:
      // RTP AUDIO PACKET
        audioRR.ssrc = head->getSSRC();
        if (!rtpSequenceLessThan(seqNum, audioRR.last_rtp_seq_num)) {
          if (seqNum < audioRR.last_rtp_seq_num) {
            ELOG_WARN("VIDEO RTP WRAPPED");
            audioRR.cycle++;
          }
          audioRR.last_rtp_seq_num = seqNum;
        }
        break;
        default:
        break;
    }
  } else {
    // RTCP PACKETS
    if (chead->getSSRC() == videoRR.ssrc && chead->packettype == RTCP_Sender_PT) {
      // VIDEO SR
      videoRR.last_sr_mid_ntp = chead->get32MiddleNtp();
      videoRR.last_sr_recv_ts = packet->received_time_ms;
    }
    if (chead->getSSRC() == audioRR.ssrc && chead->packettype == RTCP_Sender_PT) {
      // AUDIO SR
      audioRR.last_sr_mid_ntp = chead->get32MiddleNtp();
      audioRR.last_sr_recv_ts = packet->received_time_ms;
    }
  }

  if (videoRR.ssrc != 0 && (now - videoRR.last_rr_sent_ts > kRRSendIntervalMs)) {
    // SEND A VIDEO RR
    uint32_t highestSeq = videoRR.cycle;
    highestSeq = (highestSeq << 16) | videoRR.last_rtp_seq_num;
    RtcpHeader rtcpHead;
    rtcpHead.setPacketType(RTCP_Receiver_PT);
    rtcpHead.setSSRC(videoRR.ssrc);
    rtcpHead.setSourceSSRC(videoRR.ssrc);
    rtcpHead.setFractionLost(0*256);
    rtcpHead.setLostPackets(0);
    rtcpHead.setHighestSeqnum(highestSeq);
    rtcpHead.setSeqnumCycles(videoRR.cycle);
    rtcpHead.setJitter(0);
    rtcpHead.setLastSr(videoRR.last_sr_mid_ntp);
    rtcpHead.setDelaySinceLastSr(now - videoRR.last_sr_recv_ts);
    rtcpHead.setLength(7);
    rtcpHead.setBlockCount(1);
    int length = (rtcpHead.getLength()+1)*4;
    memcpy(packet_, reinterpret_cast<uint8_t*>(&rtcpHead), length);
    if (temp_ctx_) {
      temp_ctx_->fireWrite(std::make_shared<dataPacket>(0, reinterpret_cast<char*>(&packet_), length, OTHER_PACKET));
      ELOG_DEBUG("Sent VIDEO RR - ssrc: %ld, fracLost: always 0, lostpackets: always 0, highSeqNum: %ld, cycle: %ld, jitter: always 0, LSR: %ld, DSLR: %ld", videoRR.ssrc, highestSeq, videoRR.cycle, videoRR.last_sr_mid_ntp, videoRR.last_sr_recv_ts);  //NOLINT
      videoRR.last_rr_sent_ts = now;
    }
  }

  if (audioRR.ssrc != 0 && (now - audioRR.last_rr_sent_ts > kRRSendIntervalMs)) {
    // SEND AN AUDIO RR
    uint32_t highestSeq = audioRR.cycle;
    highestSeq = (highestSeq << 16) | audioRR.last_rtp_seq_num;
    RtcpHeader rtcpHead;
    rtcpHead.setPacketType(RTCP_Receiver_PT);
    rtcpHead.setSSRC(audioRR.ssrc);
    rtcpHead.setSourceSSRC(audioRR.ssrc);
    rtcpHead.setFractionLost(0*256);
    rtcpHead.setLostPackets(0);
    rtcpHead.setHighestSeqnum(highestSeq);
    rtcpHead.setSeqnumCycles(audioRR.cycle);
    rtcpHead.setJitter(0);
    rtcpHead.setLastSr(audioRR.last_sr_mid_ntp);
    rtcpHead.setDelaySinceLastSr(now - audioRR.last_sr_recv_ts);
    rtcpHead.setLength(7);
    rtcpHead.setBlockCount(1);
    int length = (rtcpHead.getLength()+1)*4;
    memcpy(packet_, reinterpret_cast<uint8_t*>(&rtcpHead), length);
    if (temp_ctx_) {
      temp_ctx_->fireWrite(std::make_shared<dataPacket>(0, reinterpret_cast<char*>(&packet_), length, OTHER_PACKET));
      ELOG_DEBUG("Sent AUDIO RR - ssrc: %ld, fracLost: always 0, lostpackets: always 0, highSeqNum: %ld, cycle: %ld, jitter: always 0, LSR: %ld, DSLR: %ld", audioRR.ssrc, highestSeq, audioRR.cycle, audioRR.last_sr_mid_ntp, audioRR.last_sr_recv_ts);  //NOLINT
      audioRR.last_rr_sent_ts = now;
    }
  }

  ctx->fireRead(packet);
}

bool BandwidthEstimationHandler::parsePacket(std::shared_ptr<dataPacket> packet) {
  const uint8_t* buffer = reinterpret_cast<uint8_t*>(packet->data);
  size_t length = packet->length;
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(buffer, length);
  memset(&header_, 0, sizeof(header_));
  RtpHeaderExtensionMap map = getHeaderExtensionMap(packet);
  return rtp_parser.Parse(&header_, &map);
}

RtpHeaderExtensionMap BandwidthEstimationHandler::getHeaderExtensionMap(std::shared_ptr<dataPacket> packet) const {
  RtpHeaderExtensionMap map;
  switch (packet->type) {
    case VIDEO_PACKET:
      return ext_map_video_;
      break;
    case AUDIO_PACKET:
      return ext_map_audio_;
      break;
    default:
      ELOG_INFO("Won't process RTP extensions for unknown type packets");
      return {};
      break;
  }
}

void BandwidthEstimationHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  ctx->fireWrite(packet);
}

void BandwidthEstimationHandler::pickEstimatorFromHeader() {
  if (header_.extension.hasAbsoluteSendTime) {
    if (!using_absolute_send_time_) {
      using_absolute_send_time_ = true;
      pickEstimator();
    }
    packets_since_absolute_send_time_ = 0;
  } else {
    if (using_absolute_send_time_) {
      ++packets_since_absolute_send_time_;
      if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
        using_absolute_send_time_ = false;
        pickEstimator();
      }
    }
  }
}

void BandwidthEstimationHandler::pickEstimator() {
  rbe_ = picker_->pickEstimator(using_absolute_send_time_, clock_, this);
  rbe_->SetMinBitrate(min_bitrate_bps_);
}

void BandwidthEstimationHandler::sendREMBPacket() {
  remb_packet_.setPacketType(RTCP_PS_Feedback_PT);
  remb_packet_.setBlockCount(RTCP_AFB);
  memcpy(&remb_packet_.report.rembPacket.uniqueid, "REMB", 4);

  remb_packet_.setSSRC(connection_->getVideoSinkSSRC());
  remb_packet_.setSourceSSRC(connection_->getVideoSourceSSRC());
  remb_packet_.setLength(5);
  remb_packet_.setREMBBitRate(bitrate_);
  remb_packet_.setREMBNumSSRC(1);
  remb_packet_.setREMBFeedSSRC(connection_->getVideoSourceSSRC());
  int remb_length = (remb_packet_.getLength() + 1) * 4;
  if (temp_ctx_) {
    ELOG_TRACE("BWE Estimation is %d", last_send_bitrate_);
    temp_ctx_->fireWrite(std::make_shared<dataPacket>(0,
      reinterpret_cast<char*>(&remb_packet_), remb_length, OTHER_PACKET));
  }
}

void BandwidthEstimationHandler::OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                     uint32_t bitrate) {
  if (last_send_bitrate_ > 0) {
    unsigned int new_remb_bitrate = last_send_bitrate_ - bitrate_ + bitrate;
    if (new_remb_bitrate < kSendThresholdPercent * last_send_bitrate_ / 100) {
      // The new bitrate estimate is less than kSendThresholdPercent % of the
      // last report. Send a REMB asap.
      last_remb_time_ = ClockUtils::timePointToMs(clock::now()) - kRembSendIntervallMs;
    }
  }

  if (bitrate < kRembMinimumBitrate) {
    bitrate = kRembMinimumBitrate;
  }

  bitrate_ = bitrate;

  uint64_t now = ClockUtils::timePointToMs(clock::now());

  if (now - last_remb_time_ < kRembSendIntervallMs) {
    return;
  }
  last_remb_time_ = now;
  last_send_bitrate_ = bitrate_;
  sendREMBPacket();
}
}  // namespace erizo
