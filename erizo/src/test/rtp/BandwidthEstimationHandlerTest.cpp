#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>
#include <thread>  // NOLINT

#include <rtp/RtpAudioMuteHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

#include <vector>

static constexpr uint16_t kVideoSsrc = 1;
static constexpr uint16_t kAudioSsrc = 2;
static constexpr uint16_t kArbitrarySeqNumber = 12;

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using erizo::dataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::BandwidthEstimationHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using erizo::RemoteBitrateEstimatorPicker;
using std::queue;
using webrtc::RemoteBitrateObserver;
using webrtc::RemoteBitrateEstimator;

class MockWebRtcConnection: public WebRtcConnection {
 public:
  MockWebRtcConnection(std::shared_ptr<Worker> worker, const IceConfig &ice_config,
                       const std::vector<RtpMap> rtp_mappings) :
    WebRtcConnection(worker, "", ice_config, rtp_mappings, nullptr) {}

  virtual ~MockWebRtcConnection() {
  }
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

class Reader : public InboundHandler {
 public:
  MOCK_METHOD2(read, void(Context*, std::shared_ptr<dataPacket>));
};

class Writer : public OutboundHandler {
 public:
  MOCK_METHOD2(write, void(Context*, std::shared_ptr<dataPacket>));
};

class BandwidthEstimationHandlerTest : public ::testing::Test {
 public:
  BandwidthEstimationHandlerTest() : ice_config(), estimator() {}

 protected:
  virtual void SetUp() {
    picker = std::make_shared<MockRemoteBitrateEstimatorPicker>();
    EXPECT_CALL(*picker.get(), pickEstimatorProxy(_, _, _))
      .WillRepeatedly(Return(new RemoteBitrateEstimatorProxy(&estimator)));
    scheduler = std::make_shared<Scheduler>(1);
    worker = std::make_shared<Worker>(scheduler);
    worker->start();
    connection = std::make_shared<MockWebRtcConnection>(worker, ice_config, rtp_maps);

    connection->setVideoSinkSSRC(kVideoSsrc);
    connection->setAudioSinkSSRC(kAudioSsrc);

    pipeline = Pipeline::create();
    bwe_handler = std::make_shared<BandwidthEstimationHandler>(connection.get(), worker, picker);
    reader = std::make_shared<Reader>();
    writer = std::make_shared<Writer>();

    pipeline->addBack(writer);
    pipeline->addBack(bwe_handler);
    pipeline->addBack(reader);
    pipeline->finalize();
  }

  virtual void TearDown() {
  }

  std::shared_ptr<dataPacket> createDataPacket(uint16_t seq_number, packetType type) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setSeqNumber(seq_number);

    if (type == AUDIO_PACKET) {
      header->setSSRC(kAudioSsrc);
    } else {
      header->setSSRC(kVideoSsrc);
    }

    return std::make_shared<dataPacket>(0, reinterpret_cast<char*>(header), sizeof(erizo::RtpHeader), type);
  }

  IceConfig ice_config;
  std::vector<RtpMap> rtp_maps;
  std::shared_ptr<MockWebRtcConnection> connection;
  Pipeline::Ptr pipeline;
  std::shared_ptr<Reader> reader;
  std::shared_ptr<Writer> writer;
  std::shared_ptr<BandwidthEstimationHandler> bwe_handler;
  std::shared_ptr<Worker> worker;
  std::shared_ptr<Scheduler> scheduler;
  std::queue<std::shared_ptr<dataPacket>> packet_queue;
  std::shared_ptr<MockRemoteBitrateEstimatorPicker> picker;
  MockRemoteBitrateEstimator estimator;
};

MATCHER_P(HasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)->data))->getSeqNumber() == seq_num;
}

MATCHER_P(RembHasBitrateValue, bitrate, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getREMBBitRate() == bitrate;
}

TEST_F(BandwidthEstimationHandlerTest, basicBehaviourShouldWritePackets) {
  auto packet1 = createDataPacket(kArbitrarySeqNumber, AUDIO_PACKET);
  auto packet2 = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(2);
  pipeline->write(packet1);
  pipeline->write(packet2);
}

TEST_F(BandwidthEstimationHandlerTest, basicBehaviourShouldReadPackets) {
  auto packet1 = createDataPacket(kArbitrarySeqNumber, AUDIO_PACKET);
  auto packet2 = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(estimator, Process());
  EXPECT_CALL(estimator, TimeUntilNextProcess()).WillRepeatedly(Return(1000));
  EXPECT_CALL(estimator, IncomingPacket(_, _, _));

  EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(2);
  pipeline->read(packet1);
  pipeline->read(packet2);
}

TEST_F(BandwidthEstimationHandlerTest, shouldSendRembPacketWithEstimatedBitrate) {
  uint32_t kArbitraryBitrate = 100000;
  auto packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(estimator, Process());
  EXPECT_CALL(estimator, TimeUntilNextProcess()).WillRepeatedly(Return(1000));
  EXPECT_CALL(estimator, IncomingPacket(_, _, _));
  EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);

  EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(RembHasBitrateValue(kArbitraryBitrate))).Times(1);
  picker->observer_->OnReceiveBitrateChanged(std::vector<uint32_t>(), kArbitraryBitrate);
}
