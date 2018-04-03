#ifndef ERIZO_SRC_ERIZO_RTP_STATSHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_STATSHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "./Stats.h"

namespace erizo {

constexpr duration kRateStatIntervalSize = std::chrono::milliseconds(100);
constexpr uint32_t kRateStatIntervals = 30;

class MediaStream;

class StatsCalculator {
  DECLARE_LOGGER();

 public:
  StatsCalculator() : stream_{nullptr} {}
  virtual ~StatsCalculator() {}

  void update(MediaStream *connection, std::shared_ptr<Stats> stats);
  void processPacket(std::shared_ptr<DataPacket> packet);

  StatNode& getStatsInfo() {
    return stats_->getNode();
  }

  void notifyStats() {
    stats_->sendStats();
  }

 private:
  void processRtpPacket(std::shared_ptr<DataPacket> packet);
  void processRtcpPacket(std::shared_ptr<DataPacket> packet);
  void incrStat(uint32_t ssrc, std::string stat);

 private:
  MediaStream* stream_;
  std::shared_ptr<Stats> stats_;
};

class IncomingStatsHandler: public InboundHandler, public StatsCalculator {
  DECLARE_LOGGER();

 public:
  IncomingStatsHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "incoming-stats";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  MediaStream* stream_;
};

class OutgoingStatsHandler: public OutboundHandler, public StatsCalculator {
  DECLARE_LOGGER();

 public:
  OutgoingStatsHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "outgoing-stats";
  }

  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  MediaStream* stream_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_STATSHANDLER_H_
