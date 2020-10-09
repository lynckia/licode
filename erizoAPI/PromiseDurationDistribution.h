#ifndef ERIZOAPI_PROMISEDURATIONDISTRIBUTION_H_
#define ERIZOAPI_PROMISEDURATIONDISTRIBUTION_H_

#include <lib/Clock.h>

typedef unsigned int uint;

using erizo::duration;

class PromiseDurationDistribution {
 public:
  PromiseDurationDistribution();
  ~PromiseDurationDistribution() {}
  void reset();
  void add(duration promise_duration);
  PromiseDurationDistribution& operator+=(const PromiseDurationDistribution& buf);

 public:
  uint duration_0_10_ms;
  uint duration_10_50_ms;
  uint duration_50_100_ms;
  uint duration_100_1000_ms;
  uint duration_1000_ms;
};

#endif  // ERIZOAPI_PROMISEDURATIONDISTRIBUTION_H_
