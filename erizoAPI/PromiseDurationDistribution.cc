#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "PromiseDurationDistribution.h"

PromiseDurationDistribution::PromiseDurationDistribution()
    : duration_0_10_ms{0},
      duration_10_50_ms{0},
      duration_50_100_ms{0},
      duration_100_1000_ms{0},
      duration_1000_ms{0} {}

void PromiseDurationDistribution::reset() {
  duration_0_10_ms     = 0;
  duration_10_50_ms    = 0;
  duration_50_100_ms   = 0;
  duration_100_1000_ms = 0;
  duration_1000_ms     = 0;
}

PromiseDurationDistribution& PromiseDurationDistribution::operator+=(const PromiseDurationDistribution& durations) {
  duration_0_10_ms     += durations.duration_0_10_ms;
  duration_10_50_ms    += durations.duration_10_50_ms;
  duration_50_100_ms   += durations.duration_50_100_ms;
  duration_100_1000_ms += durations.duration_100_1000_ms;
  duration_1000_ms     += durations.duration_1000_ms;
  return *this;
}

void PromiseDurationDistribution::add(duration promise_duration) {
  if (promise_duration <= std::chrono::milliseconds(10)) {
    duration_0_10_ms++;
  } else if (promise_duration <= std::chrono::milliseconds(50)) {
    duration_10_50_ms++;
  } else if (promise_duration <= std::chrono::milliseconds(100)) {
    duration_50_100_ms++;
  } else if (promise_duration <= std::chrono::milliseconds(1000)) {
    duration_100_1000_ms++;
  } else {
    duration_1000_ms++;
  }
}
