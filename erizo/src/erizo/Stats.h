/*
 * Stats.h
 */
#ifndef ERIZO_SRC_ERIZO_STATS_H_
#define ERIZO_SRC_ERIZO_STATS_H_

#include <string>
#include <map>

#include "./logger.h"
#include "pipeline/Service.h"
#include "rtp/RtpHeaders.h"
#include "lib/Clock.h"

#include "stats/StatNode.h"

namespace erizo {

class WebRtcConnectionStatsListener;

class Stats : public Service {
  DECLARE_LOGGER();

 public:
  Stats();
  virtual ~Stats();

  StatNode& getNode();

  std::string getStats();

  inline void setStatsListener(WebRtcConnectionStatsListener* listener) {
    listener_ = listener;
  }

  void sendStats();

 private:
  WebRtcConnectionStatsListener* listener_;
  StatNode root_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_STATS_H_
