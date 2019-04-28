#ifndef ERIZO_SRC_ERIZO_RTP_BANDWIDTHESTIMATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_BANDWIDTHESTIMATIONHANDLER_H_

#include <array>
#include <vector>
#include <string>
#include <atomic>

#include "./logger.h"
#include "./Stats.h"
#include "pipeline/Handler.h"
#include "rtp/RtpExtensionProcessor.h"

#include "thread/Worker.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace erizo {

class MediaStream;
using webrtc::RemoteBitrateEstimator;
using webrtc::RemoteBitrateObserver;
using webrtc::RtpHeaderExtensionMap;

class RemoteBitrateEstimatorPicker {
 public:
  virtual std::unique_ptr<RemoteBitrateEstimator> pickEstimator(bool using_absolute_send_time,
    webrtc::Clock* const clock, RemoteBitrateObserver *observer);
  virtual ~RemoteBitrateEstimatorPicker() {}
};

class BandwidthEstimationHandler: public Handler, public RemoteBitrateObserver,
                public std::enable_shared_from_this<BandwidthEstimationHandler> {
  DECLARE_LOGGER();

 public:
  static const uint32_t kRembMinimumBitrate;

  explicit BandwidthEstimationHandler(
    std::shared_ptr<RemoteBitrateEstimatorPicker> picker = std::make_shared<RemoteBitrateEstimatorPicker>());

  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                       uint32_t bitrate) override;

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "bwe";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

  void updateExtensionMaps(std::array<RTPExtensions, 15> video_map, std::array<RTPExtensions, 15> audio_map);

 private:
  void process();
  void sendREMBPacket();
  bool parsePacket(std::shared_ptr<DataPacket> packet);
  RtpHeaderExtensionMap getHeaderExtensionMap(std::shared_ptr<DataPacket> packet) const;
  void pickEstimatorFromHeader();
  void pickEstimator();

  void updateExtensionMap(bool video, std::array<RTPExtensions, 15> map);

  MediaStream *stream_;
  std::shared_ptr<Worker> worker_;
  std::shared_ptr<Stats> stats_;
  webrtc::Clock* const clock_;
  std::shared_ptr<RemoteBitrateEstimatorPicker> picker_;
  std::unique_ptr<RemoteBitrateEstimator> rbe_;
  bool using_absolute_send_time_;
  uint32_t packets_since_absolute_send_time_;
  int min_bitrate_bps_;
  webrtc::RTPHeader header_;
  RtcpHeader remb_packet_;
  RtpHeaderExtensionMap ext_map_audio_, ext_map_video_;
  uint32_t bitrate_;
  uint32_t last_send_bitrate_;
  uint32_t max_video_bw_;
  uint64_t last_remb_time_;
  bool running_;
  bool active_;
  bool initialized_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_BANDWIDTHESTIMATIONHANDLER_H_
