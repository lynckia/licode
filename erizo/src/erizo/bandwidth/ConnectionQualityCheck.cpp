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
    : quality_level_{ConnectionQualityLevel::GOOD}, audio_buffer_{1}, video_buffer_{1} {
}

void ConnectionQualityCheck::onFeedback(std::shared_ptr<DataPacket> packet,
    const std::vector<std::shared_ptr<MediaStream>> &streams) {
  if (streams.size() != audio_buffer_.capacity()) {
    audio_buffer_.set_capacity(streams.size());
    video_buffer_.set_capacity(streams.size());
  }
  int reports_count = 0;
  RtpUtils::forEachRtcpBlock(packet, [streams, this, &reports_count](RtcpHeader *chead) {
    reports_count++;
    uint32_t ssrc = chead->isFeedback() ? chead->getSourceSSRC() : chead->getSSRC();
    bool is_rr = chead->isReceiverReport();
    if (!is_rr) {
      return;
    }
    uint8_t fraction_lost = chead->getFractionLost();
    std::for_each(streams.begin(), streams.end(),
        [ssrc, fraction_lost, this] (const std::shared_ptr<MediaStream> &media_stream) {
      bool is_audio = media_stream->isAudioSourceSSRC(ssrc) || media_stream->isAudioSinkSSRC(ssrc);
      bool is_video = media_stream->isVideoSourceSSRC(ssrc) || media_stream->isVideoSinkSSRC(ssrc);
      if (is_audio) {
        audio_buffer_.push_back(fraction_lost);
      } else if (is_video) {
        video_buffer_.push_back(fraction_lost);
      }
    });
  });
  maybeNotifyMediaStreamsAboutConnectionQualityLevel(streams);
}

void ConnectionQualityCheck::maybeNotifyMediaStreamsAboutConnectionQualityLevel(
    const std::vector<std::shared_ptr<MediaStream>> &streams) {
  size_t audio_buffer_size = audio_buffer_.size();
  size_t video_buffer_size = video_buffer_.size();
  if (audio_buffer_size + video_buffer_size < streams.size()) {
    return;
  }
  uint16_t total_audio_fraction_lost = 0;
  uint16_t total_video_fraction_lost = 0;
  for (uint8_t f_lost : audio_buffer_) {
    total_audio_fraction_lost += f_lost;
  }
  for (uint8_t f_lost : video_buffer_) {
    total_video_fraction_lost += f_lost;
  }
  uint8_t audio_fraction_lost = audio_buffer_size > 0 ? total_audio_fraction_lost / audio_buffer_size : 0;
  uint8_t video_fraction_lost = video_buffer_size > 0 ? total_video_fraction_lost / video_buffer_size : 0;
  ConnectionQualityLevel level = ConnectionQualityLevel::GOOD;
  if (audio_fraction_lost >= kHighAudioFractionLostThreshold) {
    level = ConnectionQualityLevel::HIGH_LOSSES;
  } else if (audio_fraction_lost >= kLowAudioFractionLostThreshold) {
    level = ConnectionQualityLevel::LOW_LOSSES;
  }
  if (video_fraction_lost >= kHighVideoFractionLostThreshold) {
    level = std::min(level, ConnectionQualityLevel::HIGH_LOSSES);
  } else if (video_fraction_lost >= kLowVideoFractionLostThreshold) {
    level = std::min(level, ConnectionQualityLevel::LOW_LOSSES);
  }
  if (level != quality_level_) {
    quality_level_ = level;
    std::for_each(streams.begin(), streams.end(), [level] (const std::shared_ptr<MediaStream> &media_stream) {
      media_stream->deliverEvent(std::make_shared<ConnectionQualityEvent>(level));
    });
  }
}

}  // namespace erizo
