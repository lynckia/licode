/*
 * Stats.cpp
 *
 */

#include <sstream>
#include <string>

#include "Stats.h"
#include "MediaStream.h"
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

  void Stats::setStatsListener(MediaStreamStatsListener* listener) {
    boost::mutex::scoped_lock lock(listener_mutex_);
    listener_ = listener;
  }

  void Stats::sendStats() {
    boost::mutex::scoped_lock lock(listener_mutex_);
    if (listener_) listener_->notifyStats(getStats());
  }
}  // namespace erizo
