/*
 * TargetVideoBWDistributor.cpp
 */

#include <algorithm>

#include "TargetVideoBWDistributor.h"
#include "MediaStream.h"
#include "Transport.h"
#include "rtp/RtpUtils.h"

namespace erizo {

void TargetVideoBWDistributor::distribute(uint32_t remb, uint32_t ssrc,
                                  std::vector<std::shared_ptr<MediaStream>> streams, Transport *transport) {
  std::sort(streams.begin(), streams.end(),
    [this](const std::shared_ptr<MediaStream> &i, const std::shared_ptr<MediaStream> &j) {
      if (i->isSlideShowModeEnabled()) return true;
      if (j->isSlideShowModeEnabled()) return false;
      return getTargetVideoBW(i) < getTargetVideoBW(j);
    });

  uint8_t remaining_streams = streams.size();
  uint32_t remaining_bitrate = remb;
  std::for_each(streams.begin(), streams.end(),
    [&remaining_bitrate, &remaining_streams, transport, ssrc, this](const std::shared_ptr<MediaStream> &stream) {
      uint32_t max_bitrate = stream->getMaxVideoBW();

      uint32_t target_bitrate = getTargetVideoBW(stream);

      uint32_t remaining_avg_bitrate = remaining_bitrate / remaining_streams;
      uint32_t bitrate = std::min(target_bitrate, remaining_avg_bitrate);
      uint32_t remb = std::min(max_bitrate, remaining_avg_bitrate);
      uint32_t bitrate_to_transfer = std::max(bitrate - target_bitrate * 1.2, 0.0);
      auto generated_remb = RtpUtils::createREMB(ssrc, {stream->getVideoSinkSSRC()}, remb);
      stream->onTransportData(generated_remb, transport);

      remaining_bitrate -= bitrate + bitrate_to_transfer;
      remaining_streams--;
    });
}

uint32_t TargetVideoBWDistributor::getTargetVideoBW(const std::shared_ptr<MediaStream> &stream) {
  bool slide_show_mode = stream->isSlideShowModeEnabled();
  bool is_simulcast = stream->isSimulcast();
  uint32_t bitrate_sent = stream->getBitrateSent();
  uint32_t max_bitrate = stream->getMaxVideoBW();

  uint32_t target_bitrate = max_bitrate;
  if (is_simulcast) {
    target_bitrate = std::min(stream->getBitrateFromMaxQualityLayer(), max_bitrate);
  }
  if (slide_show_mode) {
    target_bitrate = std::min(bitrate_sent, max_bitrate);
  }
  return target_bitrate;
}

}  // namespace erizo
