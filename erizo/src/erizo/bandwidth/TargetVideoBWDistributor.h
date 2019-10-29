#ifndef ERIZO_SRC_ERIZO_BANDWIDTH_TARGETVIDEOBWDISTRIBUTOR_H_
#define ERIZO_SRC_ERIZO_BANDWIDTH_TARGETVIDEOBWDISTRIBUTOR_H_

#include "bandwidth/BandwidthDistributionAlgorithm.h"

namespace erizo {

class MediaStream;

struct MediaStreamInfo {
 public:
  std::shared_ptr<MediaStream> stream;
  bool is_simulcast;
  bool is_slideshow;
  uint32_t bitrate_sent;
  uint32_t max_video_bw;
  uint32_t bitrate_from_max_quality_layer;
  uint32_t target_video_bitrate;
};

class TargetVideoBWDistributor : public BandwidthDistributionAlgorithm {
 public:
  TargetVideoBWDistributor() {}
  virtual ~TargetVideoBWDistributor() {}
  void distribute(uint32_t remb, uint32_t ssrc, std::vector<std::shared_ptr<MediaStream>> streams,
                  Transport *transport) override;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_BANDWIDTH_TARGETVIDEOBWDISTRIBUTOR_H_
