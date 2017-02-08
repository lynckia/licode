#ifndef ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
#include "pipeline/Handler.h"
#include "./logger.h"
#include "./WebRtcConnection.h"
#include "./rtp/RtcpProcessor.h"

#include "webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.h"

namespace erizo {
using webrtc::SendSideBandwidthEstimation;

class SenderBandwidthEstimationHandler : public Handler,
  public std::enable_shared_from_this<SenderBandwidthEstimationHandler> {
  DECLARE_LOGGER();

 public:
  SenderBandwidthEstimationHandler();
  explicit SenderBandwidthEstimationHandler(const SenderBandwidthEstimationHandler&& handler);  // NOLINT
  virtual ~SenderBandwidthEstimationHandler() {}

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "sender_bwe";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

  void analyzeSr(RtcpHeader *head);

 private:
  static const uint16_t kMaxSrListSize = 20;
  WebRtcConnection* connection_;
  bool initialized_;
  bool enabled_;
  uint32_t packets_sent_;
  std::shared_ptr<SendSideBandwidthEstimation> sender_bwe_;
  std::list<std::shared_ptr<SrDelayData>> sr_delay_data_;
};
}  // namespace erizo
#endif  //  ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
