#ifndef ERIZO_SRC_ERIZO_BANDWIDTH_MAXVIDEOBWDISTRIBUTOR_H_
#define ERIZO_SRC_ERIZO_BANDWIDTH_MAXVIDEOBWDISTRIBUTOR_H_

#include "bandwidth/BandwidthDistributionAlgorithm.h"

namespace erizo {

class MaxVideoBWDistributor : public BandwidthDistributionAlgorithm {
 public:
  MaxVideoBWDistributor() {}
  virtual ~MaxVideoBWDistributor() {}
  void distribute(uint32_t remb, uint32_t ssrc, std::vector<std::shared_ptr<MediaStream>> streams,
                  Transport *transport) override;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_BANDWIDTH_MAXVIDEOBWDISTRIBUTOR_H_
