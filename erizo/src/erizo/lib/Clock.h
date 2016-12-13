#ifndef ERIZO_SRC_ERIZO_LIB_CLOCK_H_
#define ERIZO_SRC_ERIZO_LIB_CLOCK_H_

#include <chrono>  // NOLINT

namespace erizo {

using clock = std::chrono::steady_clock;
using time_point = std::chrono::steady_clock::time_point;
using duration = std::chrono::steady_clock::duration;

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_LIB_CLOCK_H_
