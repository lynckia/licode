#ifndef ERIZO_SRC_ERIZO_BANDWIDTH_BANDWIDTHDISTRIBUTIONALGORITHM_H_
#define ERIZO_SRC_ERIZO_BANDWIDTH_BANDWIDTHDISTRIBUTIONALGORITHM_H_

#include <memory>
#include <vector>

namespace erizo {

class MediaStream;
class Transport;

class BandwidthDistributionAlgorithm {
 public:
  BandwidthDistributionAlgorithm() {}
  virtual ~BandwidthDistributionAlgorithm() {}
  virtual void distribute(uint32_t remb, uint32_t ssrc, std::vector<std::shared_ptr<MediaStream>> streams,
                          Transport *transport) = 0;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_BANDWIDTH_BANDWIDTHDISTRIBUTIONALGORITHM_H_
