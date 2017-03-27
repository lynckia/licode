#ifndef ERIZO_SRC_ERIZO_LIB_CLOCKUTILS_H_
#define ERIZO_SRC_ERIZO_LIB_CLOCKUTILS_H_

#include <chrono>  // NOLINT

#include "Clock.h"

namespace erizo {

class ClockUtils {
 public:
  static inline int64_t durationToMs(erizo::duration duration) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  }

  static inline uint64_t timePointToMs(erizo::time_point time_point) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count();
  }
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_LIB_CLOCKUTILS_H_
