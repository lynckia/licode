#include "rtp/BandwidthEstimationHandler.h"

#include <vector>

#include "./WebRtcConnection.h"

#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"

namespace erizo {

using webrtc::RemoteBitrateEstimatorSingleStream;
using webrtc::RemoteBitrateEstimatorAbsSendTime;

DEFINE_LOGGER(BandwidthEstimationHandler, "rtp.BandwidthEstimationHandler");

static const uint32_t kTimeOffsetSwitchThreshold = 30;
static const uint32_t kMinBitRateAllowed = 10;

BandwidthEstimationHandler::BandwidthEstimationHandler() :
  clock_{webrtc::Clock::GetRealTimeClock()},
  rbe_{new RemoteBitrateEstimatorSingleStream(this, clock_)},
  using_absolute_send_time_{false}, packets_since_absolute_send_time_{0},
  min_bitrate_bps_{kMinBitRateAllowed} {
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
        type = webrtc::kRtpExtensionNone;
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
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (!chead->isRtcp()) {
    int64_t arrival_time_ms = clock_->CurrentNtpInMilliseconds();
    size_t payload_size = packet->length;
    if (parsePacket(packet)) {
      pickEstimatorFromHeader();
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

void BandwidthEstimationHandler::OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                     uint32_t bitrate) {

  ELOG_DEBUG("BWE Extimation is %d", bitrate);
}
}  // namespace erizo
