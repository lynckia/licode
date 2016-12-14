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
const int kRembSendIntervallMs = 1000;
const unsigned int kRembMinimumBitrateKbps = 50;

// % threshold for if we should send a new REMB asap.
const unsigned int kSendThresholdPercent = 97;

BandwidthEstimationHandler::BandwidthEstimationHandler(WebRtcConnection *connection, std::shared_ptr<Worker> worker) :
  connection_{connection},
  worker_{worker},
  clock_{webrtc::Clock::GetRealTimeClock()},
  rbe_{new RemoteBitrateEstimatorSingleStream(this, clock_)},
  using_absolute_send_time_{false}, packets_since_absolute_send_time_{0},
  min_bitrate_bps_{kMinBitRateAllowed}, running_{false} {
}

void BandwidthEstimationHandler::process() {
  rbe_->Process();
  std::weak_ptr<BandwidthEstimationHandler> weak_ptr = shared_from_this();
  worker_->scheduleFromNow([weak_ptr]() {
    if (auto this_ptr = weak_ptr.lock()) {
      this_ptr->process();
    }
  }, std::chrono::milliseconds(rbe_->TimeUntilNextProcess()));

  uint64_t now = ClockUtils::timePointToMs(clock::now());
  if (now - last_remb_time_ < kRembSendIntervallMs) {
    return;
  }

  last_remb_time_ = now;

  if (now - bitrate_update_time_ms_ > kRembTimeOutThresholdMs) {
    bitrate_ = 0;
    bitrate_update_time_ms_ = -1;
  }

  last_send_bitrate_ = bitrate_;

  if (last_send_bitrate_ < kRembMinimumBitrateKbps) {
    last_send_bitrate_ = kRembMinimumBitrateKbps;
  }

  sendREMBPacket();
}

void BandwidthEstimationHandler::updateExtensionMaps(std::array<RTPExtensions, 10> video_map,
                                                     std::array<RTPExtensions, 10> audio_map) {
  updateExtensionMap(true, video_map);
  updateExtensionMap(false, audio_map);
}

void BandwidthEstimationHandler::updateExtensionMap(bool video, std::array<RTPExtensions, 10> map) {
  webrtc::RTPExtensionType type;
  uint8_t id = 0;
  for (RTPExtensions extension : map) {
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
    if (video) {
      ext_map_video_.RegisterByType(id++, type);
    } else {
      ext_map_audio_.RegisterByType(id++, type);
    }
  }
}

void BandwidthEstimationHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  if (!running_) {
    process();
    running_ = true;
  }
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (!chead->isRtcp()) {
    int64_t arrival_time_ms = ClockUtils::timePointToMs(clock::now());
    size_t payload_size = packet->length;
    if (parsePacket(packet)) {
      pickEstimatorFromHeader();
      temp_ctx_ = ctx;
      rbe_->IncomingPacket(arrival_time_ms, payload_size, header_);
    }
    ctx->fireRead(packet);
  }
}

bool BandwidthEstimationHandler::parsePacket(std::shared_ptr<dataPacket> packet) {
  const uint8_t* buffer = reinterpret_cast<uint8_t*>(packet->data);
  size_t length = packet->length;
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(buffer, length);
  memset(&header_, 0, sizeof(header_));
  RtpHeaderExtensionMap map = getHeaderExtensionMap(packet);
  const bool valid_rtpheader = rtp_parser.Parse(&header_, &map);
  if (!valid_rtpheader) {
    return false;
  }
  return true;
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
  if (header_.extension.hasAbsoluteSendTime != 0) {
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
  if (using_absolute_send_time_) {
    rbe_.reset(new RemoteBitrateEstimatorAbsSendTime(this, clock_));
  } else {
    rbe_.reset(new RemoteBitrateEstimatorSingleStream(this, clock_));
  }
  rbe_->SetMinBitrate(min_bitrate_bps_);
}

void BandwidthEstimationHandler::sendREMBPacket() {
  remb_packet_.setPacketType(RTCP_PS_Feedback_PT);
  remb_packet_.setBlockCount(RTCP_AFB);
  memcpy(&remb_packet_.report.rembPacket.uniqueid, "REMB", 4);

  remb_packet_.setSSRC(connection_->getVideoSinkSSRC());
  remb_packet_.setSourceSSRC(connection_->getVideoSourceSSRC());
  remb_packet_.setLength(5);
  remb_packet_.setREMBBitRate(last_send_bitrate_);
  remb_packet_.setREMBNumSSRC(1);
  remb_packet_.setREMBFeedSSRC(connection_->getVideoSourceSSRC());
  int remb_length = (remb_packet_.getLength() + 1) * 4;
  temp_ctx_->fireWrite(std::make_shared<dataPacket>(0,
    reinterpret_cast<char*>(&remb_packet_), remb_length, OTHER_PACKET));
}

void BandwidthEstimationHandler::OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                     uint32_t bitrate) {
  ELOG_DEBUG("BWE Estimation is %d", bitrate);
  if (last_send_bitrate_ > 0) {
      unsigned int new_remb_bitrate = last_send_bitrate_ - bitrate_ + bitrate;

      if (new_remb_bitrate < kSendThresholdPercent * last_send_bitrate_ / 100) {
        // The new bitrate estimate is less than kSendThresholdPercent % of the
        // last report. Send a REMB asap.
        last_remb_time_ = ClockUtils::timePointToMs(clock::now()) - kRembSendIntervallMs;
      }
    }
    bitrate_ = bitrate;
    bitrate_update_time_ms_ = ClockUtils::timePointToMs(clock::now());
}
}  // namespace erizo
