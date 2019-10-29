#include "rtp/BandwidthEstimationHandler.h"

#include <vector>

#include "./MediaStream.h"
#include "lib/Clock.h"
#include "lib/ClockUtils.h"
#include "./DefaultValues.h"

#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "webrtc/base/logging.h"

namespace erizo {

using webrtc::RemoteBitrateEstimatorSingleStream;
using webrtc::RemoteBitrateEstimatorAbsSendTime;

DEFINE_LOGGER(BandwidthEstimationHandler, "rtp.BandwidthEstimationHandler");

static const uint32_t kTimeOffsetSwitchThreshold = 30;
static const uint32_t kMinBitRateAllowed = 10;
const int kRembSendIntervallMs = 200;
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

BandwidthEstimationHandler::BandwidthEstimationHandler(std::shared_ptr<RemoteBitrateEstimatorPicker> picker) :
  stream_{nullptr}, clock_{webrtc::Clock::GetRealTimeClock()},
  picker_{picker},
  using_absolute_send_time_{false}, packets_since_absolute_send_time_{0},
  min_bitrate_bps_{kMinBitRateAllowed},
  bitrate_{0}, last_send_bitrate_{0}, max_video_bw_{kDefaultMaxVideoBWInKbps}, last_remb_time_{0},
  running_{false}, active_{true}, initialized_{false} {
    rtc::LogMessage::SetLogToStderr(false);
}

void BandwidthEstimationHandler::enable() {
  active_ = true;
}

void BandwidthEstimationHandler::disable() {
  active_ = false;
}

void BandwidthEstimationHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();

  if (pipeline) {
    auto rtcp_processor = pipeline->getService<RtcpProcessor>();
    if (rtcp_processor) {
      max_video_bw_ = rtcp_processor->getMaxVideoBW();
    }
  }

  if (initialized_) {
    return;
  }

  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
  }
  if (!stream_) {
    return;
  }
  worker_ = stream_->getWorker();
  stats_ = pipeline->getService<Stats>();
  RtpExtensionProcessor& ext_processor = stream_->getRtpExtensionProcessor();
  if (ext_processor.getVideoExtensionMap().size() == 0) {
    return;
  }
  updateExtensionMaps(ext_processor.getVideoExtensionMap(), ext_processor.getAudioExtensionMap());

  pickEstimator();
  initialized_ = true;
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

void BandwidthEstimationHandler::updateExtensionMaps(std::array<RTPExtensions, 15> video_map,
                                                     std::array<RTPExtensions, 15> audio_map) {
  updateExtensionMap(true, video_map);
  updateExtensionMap(false, audio_map);
}

void BandwidthEstimationHandler::updateExtensionMap(bool is_video, std::array<RTPExtensions, 15> map) {
  webrtc::RTPExtensionType type = webrtc::kRtpExtensionNone;
  for (uint8_t id = 0; id < 15; id++) {
    RTPExtensions extension = map[id];
    switch (extension) {
      case RTP_ID:
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
      case TRANSPORT_CC:
        type = webrtc::kRtpExtensionTransportSequenceNumber;
        break;
      case PLAYBACK_TIME:
        type = webrtc::kRtpExtensionPlayoutDelay;
        break;
    }
    if (type == webrtc::kRtpExtensionNone) {
      continue;
    }
    if (is_video) {
      ext_map_video_.RegisterByType(id, type);
    } else {
      ext_map_audio_.RegisterByType(id, type);
    }
  }
}

void BandwidthEstimationHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  if (!initialized_) {
    ctx->fireRead(std::move(packet));
    return;
  }
  if (initialized_ && !running_) {
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
  ctx->fireRead(std::move(packet));
}

bool BandwidthEstimationHandler::parsePacket(std::shared_ptr<DataPacket> packet) {
  const uint8_t* buffer = reinterpret_cast<uint8_t*>(packet->data);
  size_t length = packet->length;
  webrtc::RtpUtility::RtpHeaderParser rtp_parser(buffer, length);
  memset(&header_, 0, sizeof(header_));
  RtpHeaderExtensionMap map = getHeaderExtensionMap(packet);
  return rtp_parser.Parse(&header_, &map);
}

RtpHeaderExtensionMap BandwidthEstimationHandler::getHeaderExtensionMap(std::shared_ptr<DataPacket> packet) const {
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

void BandwidthEstimationHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireWrite(std::move(packet));
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

  remb_packet_.setSSRC(stream_->getVideoSinkSSRC());
  //  todo(pedro) figure out which sourceSSRC to use here
  remb_packet_.setSourceSSRC(stream_->getVideoSourceSSRC());
  remb_packet_.setLength(5);
  uint32_t capped_bitrate = max_video_bw_ > 0 ? std::min(max_video_bw_, bitrate_) : bitrate_;
  ELOG_DEBUG("Bitrates min(%u,%u) = %u", bitrate_, max_video_bw_, capped_bitrate);
  remb_packet_.setREMBBitRate(capped_bitrate);
  remb_packet_.setREMBNumSSRC(1);
  remb_packet_.setREMBFeedSSRC(0, stream_->getVideoSourceSSRC());
  int remb_length = (remb_packet_.getLength() + 1) * 4;
  if (active_) {
    ELOG_DEBUG("BWE Estimation is %d", last_send_bitrate_);
    getContext()->fireWrite(std::make_shared<DataPacket>(0,
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
  stats_->getNode()
  [stream_->getVideoSourceSSRC()].insertStat("erizoBandwidth", CumulativeStat{last_send_bitrate_});
  sendREMBPacket();
}

}  // namespace erizo
