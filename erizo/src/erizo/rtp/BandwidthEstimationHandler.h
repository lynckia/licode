#ifndef ERIZO_SRC_ERIZO_RTP_BANDWIDTHESTIMATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_BANDWIDTHESTIMATIONHANDLER_H_

#include <array>
#include <vector>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/RtpExtensionProcessor.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace erizo {

class WebRtcConnection;

using webrtc::RemoteBitrateEstimator;
using webrtc::RemoteBitrateObserver;
using webrtc::Clock;
using webrtc::RtpHeaderExtensionMap;

class BandwidthEstimationHandler: public Handler, public RemoteBitrateObserver {
  DECLARE_LOGGER();

 public:
  BandwidthEstimationHandler();

  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                       uint32_t bitrate) override;

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;

  void updateExtensionMaps(std::array<RTPExtensions, 10> video_map, std::array<RTPExtensions, 10> audio_map);

 private:
  bool parsePacket(std::shared_ptr<dataPacket> packet);
  RtpHeaderExtensionMap getHeaderExtensionMap(std::shared_ptr<dataPacket> packet) const;
  void pickEstimatorFromHeader();
  void pickEstimator();

  void updateExtensionMap(bool video, std::array<RTPExtensions, 10> map);

  Clock* const clock_;
  std::unique_ptr<RemoteBitrateEstimator> rbe_;
  bool using_absolute_send_time_;
  uint32_t packets_since_absolute_send_time_;
  int min_bitrate_bps_;
  webrtc::RTPHeader header_;
  RtpHeaderExtensionMap ext_map_audio_, ext_map_video_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_BANDWIDTHESTIMATIONHANDLER_H_
