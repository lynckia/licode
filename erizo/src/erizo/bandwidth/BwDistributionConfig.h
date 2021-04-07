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
  StreamPriorityStrategy(const std::string& strategy_id = "empty");
  std::vector<StreamPriorityStep> strategy;
  uint16_t step_index;
  std::string strategy_id;
  void addStep(StreamPriorityStep step);
  void initWithVector(std::vector<StreamPriorityStep> strat_vector);
  int getHighestLayerForPriority(std::string priority);
  void reset() { step_index = 0; }
  bool hasNextStep();
  StreamPriorityStep getNextStep();
  std::string getStrategyId() {
    return strategy_id_;
  }

  private:
  std::string strategy_id_;
  
};

class BwDistributionConfig {
 public:
  explicit BwDistributionConfig(BwDistributorType distributor = TARGET_VIDEO_BW, const std::string& strategy_id = "empty"):
    selected_distributor{distributor}, priority_strategy{StreamPriorityStrategy(strategy_id)} {};
  BwDistributorType selected_distributor;
  StreamPriorityStrategy priority_strategy;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_BANDWIDTH_BWDISTRIBUTIONCONFIG_H_

