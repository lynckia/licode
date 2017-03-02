/*
 * Stats.cpp
 *
 */

#include <sstream>
#include <string>

#include "Stats.h"
#include "WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

  DEFINE_LOGGER(Stats, "Stats");

  Stats::Stats() : listener_{nullptr} {
  }

  Stats::~Stats() {
  }

  StatNode& Stats::getNode() {
    return root_;
  }

  std::string Stats::getStats() {
    return root_.toString();
  }

  void Stats::sendStats() {
    if (listener_) listener_->notifyStats(getStats());
  }
}  // namespace erizo
