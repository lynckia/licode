#ifndef ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
#include <map>

#include "pipeline/Handler.h"
#include "./logger.h"
#include "lib/Clock.h"
#include "rtp/RtcpProcessor.h"

#include "webrtc/api/units/timestamp.h"
#include "webrtc/modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator_interface.h"
#include "webrtc/modules/congestion_controller/goog_cc/send_side_bandwidth_estimation.h"
#include "webrtc/modules/congestion_controller/rtp/transport_feedback_adapter.h"

namespace erizo {
using webrtc::SendSideBandwidthEstimation;
using webrtc::TransportFeedbackAdapter;
using webrtc::AcknowledgedBitrateEstimatorInterface;
using webrtc::rtcp::TransportFeedback;
class WebRtcConnection;
class Stats;

class SenderBandwidthEstimationListener {
 public:
  virtual void onBandwidthEstimate(int64_t estimated_bitrate, uint8_t estimated_loss,
      int64_t estimated_rtt) = 0;
};

class SenderBandwidthEstimationHandler : public Handler,
  public std::enable_shared_from_this<SenderBandwidthEstimationHandler> {
  DECLARE_LOGGER();

 public:
  static const uint16_t kMaxSrListSize = 20;
  static const uint32_t kStartSendBitrate = 300000;
  static const uint32_t kMinSendBitrate = 30000;
  static const uint32_t kMinSendBitrateLimit = 1000000;
  static const uint32_t kMaxSendBitrate = 1000000000;
  static constexpr duration kMinUpdateEstimateInterval = std::chrono::milliseconds(25);

 public:
  explicit SenderBandwidthEstimationHandler(std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());
  explicit SenderBandwidthEstimationHandler(const SenderBandwidthEstimationHandler&& handler);  // NOLINT
  virtual ~SenderBandwidthEstimationHandler() {}

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "sender_bwe";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

  void analyzeSr(RtcpHeader *head);

  void setListener(std::weak_ptr<SenderBandwidthEstimationListener> listener) {
    bwe_listener_ = listener;
  }
 private:
  void updateNumberOfStreams();
  void updateReceiverBlockFromList();
  webrtc::Timestamp getNowTimestamp();
  void onTransportFeedbackReport(const webrtc::TransportPacketsFeedback& report);
  bool receivedFeedbackOrRemb() {
    return received_transport_feedback_ || received_remb_;
  }

 private:
  WebRtcConnection* connection_;
  std::weak_ptr<SenderBandwidthEstimationListener> bwe_listener_;
  std::shared_ptr<Clock> clock_;
  bool initialized_;
  bool enabled_;
  bool received_remb_;
  bool received_transport_feedback_;
  std::map<uint32_t, uint32_t> period_packets_sent_;
  int64_t estimated_bitrate_;
  int64_t estimated_target_;
  uint8_t estimated_loss_;
  int64_t estimated_rtt_;
  time_point last_estimate_update_;
  std::shared_ptr<SendSideBandwidthEstimation> sender_bwe_;
  std::shared_ptr<TransportFeedbackAdapter> feedback_adapter_;
  std::unique_ptr<AcknowledgedBitrateEstimatorInterface>
      acknowledged_bitrate_estimator_;
  std::list<std::shared_ptr<SrDelayData>> sr_delay_data_;
  std::list<std::shared_ptr<RrDelayData>> rr_delay_data_;
  std::shared_ptr<Stats> stats_;
  uint32_t max_rr_delay_data_size_;
  uint32_t max_sr_delay_data_size_;
  uint16_t transport_wide_seqnum_;

  void updateEstimate();
};
}  // namespace erizo
#endif  //  ERIZO_SRC_ERIZO_RTP_SENDERBANDWIDTHESTIMATIONHANDLER_H_
