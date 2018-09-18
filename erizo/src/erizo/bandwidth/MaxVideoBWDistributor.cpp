/*
 * MaxVideoBWDistributor.cpp
 */

#include <algorithm>

#include "MaxVideoBWDistributor.h"
#include "MediaStream.h"
#include "Transport.h"
#include "rtp/RtpUtils.h"

namespace erizo {

void MaxVideoBWDistributor::distribute(uint32_t remb, uint32_t ssrc,
                                  std::vector<std::shared_ptr<MediaStream>> streams, Transport *transport) {
  std::sort(streams.begin(), streams.end(),
    [](const std::shared_ptr<MediaStream> &i, const std::shared_ptr<MediaStream> &j) {
      return i->getMaxVideoBW() < j->getMaxVideoBW();
    });

  uint8_t remaining_streams = streams.size();
  uint32_t remaining_bitrate = remb;
  std::for_each(streams.begin(), streams.end(),
    [&remaining_bitrate, &remaining_streams, transport, ssrc](const std::shared_ptr<MediaStream> &stream) {
      uint32_t max_bitrate = stream->getMaxVideoBW();
      uint32_t remaining_avg_bitrate = remaining_bitrate / remaining_streams;
      uint32_t bitrate = std::min(max_bitrate, remaining_avg_bitrate);
      auto generated_remb = RtpUtils::createREMB(ssrc, {stream->getVideoSinkSSRC()}, bitrate);
      stream->onTransportData(generated_remb, transport);
      remaining_bitrate -= bitrate;
      remaining_streams--;
    });
}

}  // namespace erizo
