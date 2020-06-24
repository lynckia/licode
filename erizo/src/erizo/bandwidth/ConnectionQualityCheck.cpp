/*
 * ConnectionQualityCheck.cpp
 */
#include "bandwidth/ConnectionQualityCheck.h"

#include <algorithm>

#include "MediaStream.h"
#include "Transceiver.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(ConnectionQualityCheck, "bandwidth.ConnectionQualityCheck");

constexpr uint8_t ConnectionQualityCheck::kHighAudioFractionLostThreshold;
constexpr uint8_t ConnectionQualityCheck::kLowAudioFractionLostThreshold;
constexpr uint8_t ConnectionQualityCheck::kHighVideoFractionLostThreshold;
constexpr uint8_t ConnectionQualityCheck::kLowVideoFractionLostThreshold;
constexpr size_t  ConnectionQualityCheck::kNumberOfPacketsPerStream;

ConnectionQualityCheck::ConnectionQualityCheck()
    : quality_level_{ConnectionQualityLevel::GOOD}, audio_buffer_{1}, video_buffer_{1}, recent_packet_losses_{false} {
}

void ConnectionQualityCheck::onFeedback(std::shared_ptr<DataPacket> packet,
    const std::vector<std::shared_ptr<Transceiver>> &transceivers) {
  size_t audios_unmuted = std::count_if(transceivers.begin(), transceivers.end(),
    [](const std::shared_ptr<Transceiver> &transceiver) { return transceiver->isSending() && !transceiver->getMediaStream()->isAudioMuted();});
  size_t videos_unmuted = std::count_if(transceivers.begin(), transceivers.end(),
    [](const std::shared_ptr<Transceiver> &transceiver) { return transceiver->isSending() && !transceiver->getMediaStream()->isVideoMuted();});

  audio_buffer_.set_capacity(kNumberOfPacketsPerStream * audios_unmuted);
  video_buffer_.set_capacity(kNumberOfPacketsPerStream * videos_unmuted);

  int reports_count = 0;
  int rrs = 0;
  RtpUtils::forEachRtcpBlock(packet, [transceivers, this, &reports_count, &rrs](RtcpHeader *chead) {
    reports_count++;
    uint32_t ssrc = chead->isFeedback() ? chead->getSourceSSRC() : chead->getSSRC();
    bool is_rr = chead->isReceiverReport();
    if (!is_rr) {
      return;
    }
    rrs++;
    uint8_t fraction_lost = chead->getFractionLost();
    std::for_each(transceivers.begin(), transceivers.end(),
        [ssrc, fraction_lost, this] (const std::shared_ptr<Transceiver> &transceiver) {
      if (!transceiver->hasMediaStream()) {
        return;
      }
      const auto &media_stream = transceiver->getMediaStream();
      bool is_audio = media_stream->isAudioSourceSSRC(ssrc) || media_stream->isAudioSinkSSRC(ssrc);
      bool is_video = media_stream->isVideoSourceSSRC(ssrc) || media_stream->isVideoSinkSSRC(ssrc);
      uint8_t subscriber_fraction_lost = fraction_lost;
      uint8_t publisher_fraction_lost = is_audio ?
        media_stream->getPublisherInfo().audio_fraction_lost : media_stream->getPublisherInfo().video_fraction_lost;
      if (fraction_lost < publisher_fraction_lost) {
        subscriber_fraction_lost = 0;
      } else {
        subscriber_fraction_lost = fraction_lost - publisher_fraction_lost;
      }

      if (is_audio) {
        audio_buffer_.push_back(subscriber_fraction_lost);
      } else if (is_video) {
        video_buffer_.push_back(subscriber_fraction_lost);
      }
    });
  });
  if (rrs > 0) {
    maybeNotifyMediaStreamsAboutConnectionQualityLevel(transceivers);
  }
}

bool ConnectionQualityCheck::werePacketLossesRecently() {
  return recent_packet_losses_;
}

void ConnectionQualityCheck::maybeNotifyMediaStreamsAboutConnectionQualityLevel(
    const std::vector<std::shared_ptr<Transceiver>> &transceivers) {
  if (!audio_buffer_.full() || !video_buffer_.full()) {
    return;
  }
  size_t audio_buffer_size = audio_buffer_.size();
  size_t video_buffer_size = video_buffer_.size();
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

  if (audio_fraction_lost == 0 && video_fraction_lost == 0) {
    recent_packet_losses_ = false;
  } else {
    recent_packet_losses_ = true;
  }

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
    std::for_each(transceivers.begin(), transceivers.end(), [level] (const std::shared_ptr<Transceiver> &transceiver) {
      if (transceiver->hasMediaStream()) {
        transceiver->getMediaStream()->deliverEvent(std::make_shared<ConnectionQualityEvent>(level));
      }
    });
  }
}

}  // namespace erizo
