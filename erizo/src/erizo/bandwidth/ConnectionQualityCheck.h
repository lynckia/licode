#ifndef ERIZO_SRC_ERIZO_BANDWIDTH_CONNECTIONQUALITYCHECK_H_
#define ERIZO_SRC_ERIZO_BANDWIDTH_CONNECTIONQUALITYCHECK_H_

#include <memory>
#include <vector>
#include <boost/circular_buffer.hpp>

#include "./logger.h"
#include "./MediaDefinitions.h"

namespace erizo {

class MediaStream;
class Transport;

typedef boost::circular_buffer<uint8_t> circular_buffer;

enum ConnectionQualityLevel {
  HIGH_LOSSES = 0,
  LOW_LOSSES = 1,
  GOOD = 2
};

class ConnectionQualityEvent : public MediaEvent {
 public:
  explicit ConnectionQualityEvent(ConnectionQualityLevel level_)
    : level{level_} {}

  std::string getType() const override {
    return "ConnectionQualityEvent";
  }
  ConnectionQualityLevel level;
};

class ConnectionQualityCheck {
  DECLARE_LOGGER();

 public:
  static constexpr uint8_t kHighAudioFractionLostThreshold = 20 * 256 / 100;
  static constexpr uint8_t kLowAudioFractionLostThreshold  =  5 * 256 / 100;
  static constexpr uint8_t kHighVideoFractionLostThreshold = 20 * 256 / 100;
  static constexpr uint8_t kLowVideoFractionLostThreshold  =  5 * 256 / 100;
  static constexpr size_t  kNumberOfPacketsPerStream       = 3;

 public:
  ConnectionQualityCheck();
  virtual ~ConnectionQualityCheck() {}
  void onFeedback(std::shared_ptr<DataPacket> packet, const std::vector<std::shared_ptr<MediaStream>> &streams);
  ConnectionQualityLevel getLevel() { return quality_level_; }
  bool werePacketLossesRecently();
 private:
  void maybeNotifyMediaStreamsAboutConnectionQualityLevel(const std::vector<std::shared_ptr<MediaStream>> &streams);
 private:
  ConnectionQualityLevel quality_level_;
  circular_buffer audio_buffer_;
  circular_buffer video_buffer_;
  bool recent_packet_losses_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_BANDWIDTH_CONNECTIONQUALITYCHECK_H_
