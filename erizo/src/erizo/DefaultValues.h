/*
 * defaultvalues.h
 */
#ifndef ERIZO_SRC_ERIZO_DEFAULTVALUES_H_
#define ERIZO_SRC_ERIZO_DEFAULTVALUES_H_

namespace erizo {
  constexpr uint32_t kDefaultMaxVideoBWInKbps = 30000;
  constexpr uint32_t kDefaultMaxVideoBWInBitsps = kDefaultMaxVideoBWInKbps * 1000;
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_DEFAULTVALUES_H_
