#include <algorithm>
#include "bandwidth/BwDistributionConfig.h"

namespace erizo {

  StreamPriorityStep::StreamPriorityStep(std::string aPriority, std::string aLevel):
    priority{aPriority}, level{aLevel}  {
}
bool StreamPriorityStep::isLevelFallback() {
  return level == "fallback";
}
bool StreamPriorityStep::isLevelSlideshow() {
  return level == "slideshow";
}
int StreamPriorityStep::getSpatialLayer() {
  if (isLevelSlideshow() || isLevelFallback()) {
    return -1;
  } else {
    return std::stoi(level);
  }
}

StreamPriorityStrategy::StreamPriorityStrategy(): stepIndex{0} {
}

void StreamPriorityStrategy::addStep(StreamPriorityStep step) {
  strategy.push_back(step);
}

void StreamPriorityStrategy::initWithVector(std::vector<StreamPriorityStep> stratVector) {
  std::for_each(stratVector.begin(), stratVector.end(),
    [this](StreamPriorityStep& step) {
      this->addStep(step);
    });
}

bool StreamPriorityStrategy::hasNextStep() {
  return stepIndex <  strategy.size();
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
  return strategy[stepIndex++];
}

}  // namespace erizo
