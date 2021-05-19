#include <algorithm>
#include "bandwidth/BwDistributionConfig.h"

namespace erizo {

StreamPriorityStep::StreamPriorityStep(std::string a_priority, std::string a_level):
    priority{a_priority}, level{a_level}  {
}
bool StreamPriorityStep::isLevelFallback() {
  return level == "fallback";
}
bool StreamPriorityStep::isLevelSlideshow() {
  return level == "slideshow";
}
bool StreamPriorityStep::isLevelMax() {
  return level == "max";
}
bool StreamPriorityStep::isValid() {
  return level != "invalid";
}

int StreamPriorityStep::getSpatialLayer() {
  if (isLevelSlideshow() || isLevelFallback() || isLevelMax() || !isValid()) {
    return -1;
  } else {
    return std::stoi(level);
  }
}

StreamPriorityStrategy::StreamPriorityStrategy(const std::string& strategy_id):
  step_index{0},
  strategy_id_{strategy_id} {
}

void StreamPriorityStrategy::addStep(StreamPriorityStep step) {
  strategy.push_back(step);
}

void StreamPriorityStrategy::initWithVector(std::vector<StreamPriorityStep> strat_vector) {
  std::for_each(strat_vector.begin(), strat_vector.end(),
    [this](StreamPriorityStep& step) {
      this->addStep(step);
    });
}

bool StreamPriorityStrategy::hasNextStep() {
  return step_index <  strategy.size();
}

int StreamPriorityStrategy::getHighestLayerForPriority(std::string priority) {
  for (uint16_t i = strategy.size() - 1 ; i > 0; i--) {
    if (strategy[i].priority == priority) {
      return strategy[i].getSpatialLayer();
    }
  }
  return -1;
}

StreamPriorityStep StreamPriorityStrategy::getNextStep() {
  if (!hasNextStep()) {
    return  StreamPriorityStep("0", "invalid");
  }
  return strategy[step_index++];
}

}  // namespace erizo
