/*
 * ConnectionQualityCheck.cpp
 */
#include "bandwidth/ConnectionQualityCheck.h"

#include <algorithm>

#include "MediaStream.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(ConnectionQualityCheck, "bandwidth.ConnectionQualityCheck");

constexpr uint8_t ConnectionQualityCheck::kHighAudioFractionLostThreshold;
constexpr uint8_t ConnectionQualityCheck::kLowAudioFractionLostThreshold;

ConnectionQualityCheck::ConnectionQualityCheck()
    : quality_level_{ConnectionQualityLevel::GOOD}, buffer_{1} {
}

void ConnectionQualityCheck::onFeedback(std::shared_ptr<DataPacket> packet,
    const std::vector<std::shared_ptr<MediaStream>> &streams) {
  if (streams.size() != buffer_.capacity()) {
    buffer_.set_capacity(streams.size());
  }
  int reports_count = 0;
  RtpUtils::forEachRtcpBlock(packet, [streams, this, &reports_count](RtcpHeader *chead) {
    reports_count++;
    uint32_t ssrc = chead->isFeedback() ? chead->getSourceSSRC() : chead->getSSRC();
    bool is_rr = chead->isReceiverReport();
    if (is_rr) {
      ELOG_WARN("  RR Block_COUNT %d", chead->getBlockCount());
    } else {
      ELOG_WARN("  %d", chead->getPacketType());
    }
    uint8_t fraction_lost = is_rr ? chead->getFractionLost() : 0;
    std::for_each(streams.begin(), streams.end(),
        [ssrc, fraction_lost, is_rr, this] (const std::shared_ptr<MediaStream> &media_stream) {
      bool is_audio = media_stream->isAudioSourceSSRC(ssrc) || media_stream->isAudioSinkSSRC(ssrc);
      if (is_rr && is_audio) {
        buffer_.push_back(fraction_lost);
      }
    });
  });
  ELOG_WARN("RTCP Report Counts %d", reports_count);
  maybeNotifyMediaStreamsAboutConnectionQualityLevel(streams);
}

void ConnectionQualityCheck::maybeNotifyMediaStreamsAboutConnectionQualityLevel(
    const std::vector<std::shared_ptr<MediaStream>> &streams) {
  if (!buffer_.full()) {
    return;
  }
  uint16_t total_audio_fraction_lost = 0;
  for (uint8_t f_lost : buffer_) {
    total_audio_fraction_lost += f_lost;
  }
  uint8_t fraction_lost = total_audio_fraction_lost / buffer_.size();
  ConnectionQualityLevel level = ConnectionQualityLevel::GOOD;
  if (fraction_lost >= kHighAudioFractionLostThreshold) {
    level = ConnectionQualityLevel::HIGH_AUDIO_LOSSES;
  } else if (fraction_lost >= kLowAudioFractionLostThreshold) {
    level = ConnectionQualityLevel::LOW_AUDIO_LOSSES;
  }
  if (level != quality_level_) {
    quality_level_ = level;
    std::for_each(streams.begin(), streams.end(), [level] (const std::shared_ptr<MediaStream> &media_stream) {
      media_stream->deliverEvent(std::make_shared<ConnectionQualityEvent>(level));
    });
  }
}

}  // namespace erizo
