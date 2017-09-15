#ifndef ERIZO_SRC_TEST_UTILS_MOCKS_H_
#define ERIZO_SRC_TEST_UTILS_MOCKS_H_

#include <WebRtcConnection.h>
#include <pipeline/Handler.h>
#include <rtp/RtcpProcessor.h>
#include <rtp/QualityManager.h>
#include <rtp/FecReceiverHandler.h>
#include <rtp/BandwidthEstimationHandler.h>
#include <rtp/SenderBandwidthEstimationHandler.h>
#include <webrtc/modules/rtp_rtcp/include/ulpfec_receiver.h>

#include <string>
#include <vector>

namespace erizo {

class MockRtcpProcessor : public RtcpProcessor {
 public:
  MockRtcpProcessor() : RtcpProcessor(nullptr, nullptr) {}
  MOCK_METHOD1(addSourceSsrc, void(uint32_t));
  MOCK_METHOD1(setMaxVideoBW, void(uint32_t));
  MOCK_METHOD0(getMaxVideoBW, uint32_t());
  MOCK_METHOD1(setPublisherBW, void(uint32_t));
  MOCK_METHOD1(analyzeSr, void(RtcpHeader*));
  MOCK_METHOD2(analyzeFeedback, int(char*, int));
  MOCK_METHOD0(checkRtcpFb, void());
};

class MockQualityManager : public QualityManager {
 public:
  MockQualityManager() : QualityManager() {}
  MOCK_CONST_METHOD0(getSpatialLayer, int());
  MOCK_CONST_METHOD0(getTemporalLayer, int());
  MOCK_CONST_METHOD0(isPaddingEnabled, bool());
};

class MockMediaSink : public MediaSink {
 public:
  void close() override {
    internal_close();
  }
  MOCK_METHOD0(internal_close, void());
  MOCK_METHOD2(deliverAudioDataInternal, void(char*, int));
  MOCK_METHOD2(deliverVideoDataInternal, void(char*, int));
  MOCK_METHOD1(deliverEventInternal, void(MediaEventPtr));

 private:
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override {
    deliverAudioDataInternal(audio_packet->data, audio_packet->length);
    return 0;
  }
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override {
    deliverVideoDataInternal(video_packet->data, video_packet->length);
    return 0;
  }
  int deliverEvent_(MediaEventPtr event) override {
    deliverEventInternal(event);
    return 0;
  }
};

class MockWebRtcConnection: public WebRtcConnection {
 public:
  MockWebRtcConnection(std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker, const IceConfig &ice_config,
                       const std::vector<RtpMap> rtp_mappings) :
    WebRtcConnection(worker, io_worker, "", ice_config, rtp_mappings, std::vector<erizo::ExtMap>(), nullptr) {}

  virtual ~MockWebRtcConnection() {
  }
};

class Reader : public InboundHandler {
 public:
  MOCK_METHOD0(enable, void());
  MOCK_METHOD0(disable, void());
  MOCK_METHOD0(notifyUpdate, void());
  MOCK_METHOD0(getName, std::string());
  MOCK_METHOD2(read, void(Context*, std::shared_ptr<DataPacket>));
};

class Writer : public OutboundHandler {
 public:
  MOCK_METHOD0(enable, void());
  MOCK_METHOD0(disable, void());
  MOCK_METHOD0(notifyUpdate, void());
  MOCK_METHOD0(getName, std::string());
  MOCK_METHOD2(write, void(Context*, std::shared_ptr<DataPacket>));
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

class MockUlpfecReceiver : public webrtc::UlpfecReceiver {
 public:
  virtual ~MockUlpfecReceiver() {}

  MOCK_METHOD4(AddReceivedRedPacket, int32_t(const webrtc::RTPHeader&, const uint8_t*, size_t, uint8_t));
  MOCK_METHOD0(ProcessReceivedFec, int32_t());
  MOCK_CONST_METHOD0(GetPacketCounter, webrtc::FecPacketCounter());

  // Returns a counter describing the added and recovered packets.
  // virtual webrtc::FecPacketCounter GetPacketCounter() const {};
};

class MockSenderBandwidthEstimationListener: public erizo::SenderBandwidthEstimationListener {
 public:
  MOCK_METHOD3(onBandwidthEstimate, void(int, uint8_t, int64_t));
};

}  // namespace erizo

#endif  // ERIZO_SRC_TEST_UTILS_MOCKS_H_
