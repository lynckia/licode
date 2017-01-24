#ifndef ERIZO_SRC_TEST_UTILS_MOCKS_H_
#define ERIZO_SRC_TEST_UTILS_MOCKS_H_

#include <WebRtcConnection.h>
#include <pipeline/Handler.h>

#include <string>
#include <vector>

namespace erizo {

class MockWebRtcConnection: public WebRtcConnection {
 public:
  MockWebRtcConnection(std::shared_ptr<Worker> worker, const IceConfig &ice_config,
                       const std::vector<RtpMap> rtp_mappings) :
    WebRtcConnection(worker, "", ice_config, rtp_mappings, nullptr) {}

  virtual ~MockWebRtcConnection() {
  }
};

class Reader : public InboundHandler {
 public:
  MOCK_METHOD0(enable, void());
  MOCK_METHOD0(disable, void());
  MOCK_METHOD0(getName, std::string());
  MOCK_METHOD2(read, void(Context*, std::shared_ptr<dataPacket>));
};

class Writer : public OutboundHandler {
 public:
  MOCK_METHOD0(enable, void());
  MOCK_METHOD0(disable, void());
  MOCK_METHOD0(getName, std::string());
  MOCK_METHOD2(write, void(Context*, std::shared_ptr<dataPacket>));
};

class MockRemoteBitrateEstimatorPicker : public RemoteBitrateEstimatorPicker {
 public:
  virtual std::unique_ptr<RemoteBitrateEstimator> pickEstimator(bool using_absolute_send_time,
      webrtc::Clock* const clock, RemoteBitrateObserver *observer) {
    observer_ = observer;
    return std::unique_ptr<RemoteBitrateEstimator>(pickEstimatorProxy(using_absolute_send_time, clock, observer));
  }
  MOCK_METHOD3(pickEstimatorProxy, RemoteBitrateEstimator*(bool, webrtc::Clock* const,
    RemoteBitrateObserver*));
  RemoteBitrateObserver *observer_;
};

class RemoteBitrateEstimatorProxy : public RemoteBitrateEstimator {
 public:
  explicit RemoteBitrateEstimatorProxy(RemoteBitrateEstimator* estimator) : estimator_{estimator} {}
  virtual ~RemoteBitrateEstimatorProxy() {}

  void IncomingPacketFeedbackVector(
      const std::vector<webrtc::PacketInfo>& packet_feedback_vector) override {
    estimator_->IncomingPacketFeedbackVector(packet_feedback_vector);
  }

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const webrtc::RTPHeader& header) override {
    estimator_->IncomingPacket(arrival_time_ms, payload_size, header);
  }
  void Process() override {
    estimator_->Process();
  }
  int64_t TimeUntilNextProcess() override {
    return estimator_->TimeUntilNextProcess();
  }
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override {
    estimator_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
  }
  void RemoveStream(uint32_t ssrc) override {
    estimator_->RemoveStream(ssrc);
  }
  bool LatestEstimate(std::vector<uint32_t>* ssrcs,
                      uint32_t* bitrate_bps) const override {
    return estimator_->LatestEstimate(ssrcs, bitrate_bps);
  }
  void SetMinBitrate(int min_bitrate_bps) override {
    estimator_->SetMinBitrate(min_bitrate_bps);
  }

 private:
  RemoteBitrateEstimator *estimator_;
};

class MockRemoteBitrateEstimator : public RemoteBitrateEstimator {
 public:
  MOCK_METHOD1(IncomingPacketFeedbackVector, void(const std::vector<webrtc::PacketInfo>&));
  MOCK_METHOD3(IncomingPacket, void(int64_t, size_t, const webrtc::RTPHeader&));
  MOCK_METHOD0(Process, void());
  MOCK_METHOD0(TimeUntilNextProcess, int64_t());
  MOCK_METHOD2(OnRttUpdate, void(int64_t, int64_t));
  MOCK_METHOD1(RemoveStream, void(uint32_t));
  MOCK_CONST_METHOD2(LatestEstimate, bool(std::vector<uint32_t>*, uint32_t*));
  MOCK_METHOD1(SetMinBitrate, void(int));
};

}  // namespace erizo

#endif  // ERIZO_SRC_TEST_UTILS_MOCKS_H_
