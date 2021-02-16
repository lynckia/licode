#ifndef ERIZO_SRC_ERIZO_BANDWIDTH_BWDISTRIBUTIONCONFIG_H_
#define ERIZO_SRC_ERIZO_BANDWIDTH_BWDISTRIBUTIONCONFIG_H_

#include <vector>
#include <string>

namespace erizo {

enum BwDistributorType {
  MAX_VIDEO_BW,
  TARGET_VIDEO_BW,
  STREAM_PRIORITY,
};

class StreamPriorityStep {
 public:
  std::string priority;
  std::string level;

  StreamPriorityStep(std::string aPriority, std::string aLevel);

  bool isLevelFallback();
  bool isLevelSlideshow();
  int getSpatialLayer();
};


class StreamPriorityStrategy {
 public:
  StreamPriorityStrategy();
  std::vector<StreamPriorityStep> strategy;
  uint16_t stepIndex;
  void addStep(StreamPriorityStep step);
  void initWithVector(std::vector<StreamPriorityStep> stratVector);
  int getHighestLayerForPriority(std::string priority);
  void reset() { stepIndex = 0; }
  bool hasNextStep();
  StreamPriorityStep getNextStep();
};

class BwDistributionConfig {
 public:
  explicit BwDistributionConfig(BwDistributorType distributor = TARGET_VIDEO_BW):
    selected_distributor{distributor} {};
  BwDistributorType selected_distributor;
  StreamPriorityStrategy priority_strategy;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_BANDWIDTH_BWDISTRIBUTIONCONFIG_H_

