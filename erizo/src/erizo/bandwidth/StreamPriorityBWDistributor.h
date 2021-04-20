#ifndef ERIZO_SRC_ERIZO_BANDWIDTH_STREAMPRIORITYBWDISTRIBUTOR_H_
#define ERIZO_SRC_ERIZO_BANDWIDTH_STREAMPRIORITYBWDISTRIBUTOR_H_

#include "./logger.h"
#include "./Stats.h"
#include "bandwidth/BwDistributionConfig.h"
#include "bandwidth/BandwidthDistributionAlgorithm.h"

namespace erizo {

class MediaStream;

class MediaStreamPriorityInfo {
 public:
  std::shared_ptr<MediaStream> stream;
  uint64_t assigned_bitrate;
};

class StreamPriorityBWDistributor : public BandwidthDistributionAlgorithm {
DECLARE_LOGGER();
 public:
  explicit StreamPriorityBWDistributor(StreamPriorityStrategy strategy, std::shared_ptr<Stats> stats);
  virtual ~StreamPriorityBWDistributor() {}
  void distribute(uint32_t remb, uint32_t ssrc, std::vector<std::shared_ptr<MediaStream>> streams,
                  Transport *transport) override;
  std::string getStrategyId();

 private:
  StreamPriorityStrategy strategy_;
  std::shared_ptr<Stats> stats_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_BANDWIDTH_STREAMPRIORITYBWDISTRIBUTOR_H_
