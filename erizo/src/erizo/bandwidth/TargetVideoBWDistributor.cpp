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
  std::vector<MediaStreamInfo> stream_infos{};

  std::for_each(streams.begin(), streams.end(),
    [&stream_infos](std::shared_ptr<MediaStream> stream) {
      stream_infos.push_back({stream,
                              stream->isSimulcast(),
                              stream->isSlideShowModeEnabled(),
                              stream->getVideoBitrate(),
                              stream->getMaxVideoBW(),
                              stream->getBitrateFromMaxQualityLayer(),
                              stream->getTargetVideoBitrate()});
    });

  std::sort(stream_infos.begin(), stream_infos.end(),
    [](const MediaStreamInfo &i, const MediaStreamInfo &j) {
      return i.target_video_bitrate < j.target_video_bitrate;
    });
  uint8_t remaining_streams = streams.size();
  uint32_t remaining_bitrate = remb;
  std::for_each(stream_infos.begin(), stream_infos.end(),
    [&remaining_bitrate, &remaining_streams, transport, ssrc](const MediaStreamInfo &stream) {
      uint32_t max_bitrate = stream.max_video_bw;

      uint32_t target_bitrate = stream.target_video_bitrate;

      uint32_t remaining_avg_bitrate = remaining_bitrate / remaining_streams;
      uint32_t bitrate = std::min(target_bitrate, remaining_avg_bitrate);
      uint32_t remb = std::min(max_bitrate, remaining_avg_bitrate);
      auto generated_remb = RtpUtils::createREMB(ssrc, {stream.stream->getVideoSinkSSRC()}, remb);
      stream.stream->onTransportData(generated_remb, transport);

      remaining_bitrate -= bitrate;
      remaining_streams--;
    });
}

}  // namespace erizo
