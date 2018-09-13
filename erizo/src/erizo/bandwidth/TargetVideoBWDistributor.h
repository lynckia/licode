#ifndef ERIZO_SRC_ERIZO_BANDWIDTH_TARGETVIDEOBWDISTRIBUTOR_H_
#define ERIZO_SRC_ERIZO_BANDWIDTH_TARGETVIDEOBWDISTRIBUTOR_H_

#include "bandwidth/BandwidthDistributionAlgorithm.h"

namespace erizo {

class MediaStream;

class TargetVideoBWDistributor : public BandwidthDistributionAlgorithm {
 public:
  TargetVideoBWDistributor() {}
  virtual ~TargetVideoBWDistributor() {}
  void distribute(uint32_t remb, uint32_t ssrc, std::vector<std::shared_ptr<MediaStream>> streams,
                  Transport *transport) override;
 private:
  uint32_t getTargetVideoBW(const std::shared_ptr<MediaStream> &stream);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_BANDWIDTH_TARGETVIDEOBWDISTRIBUTOR_H_
