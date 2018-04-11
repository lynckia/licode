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

class MediaStreamStatsListener;

class Stats : public Service {
  DECLARE_LOGGER();

 public:
  Stats();
  virtual ~Stats();

  StatNode& getNode();

  std::string getStats();

  void setStatsListener(MediaStreamStatsListener* listener);
  void sendStats();

 private:
  boost::mutex listener_mutex_;
  MediaStreamStatsListener* listener_;
  StatNode root_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_STATS_H_
