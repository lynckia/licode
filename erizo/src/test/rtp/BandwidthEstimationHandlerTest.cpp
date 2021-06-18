#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/BandwidthEstimationHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

#include <queue>
#include <string>
#include <thread>  // NOLINT

#include <vector>

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using ::testing::AtLeast;
using erizo::DataPacket;
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

class BandwidthEstimationHandlerTest : public erizo::HandlerTest {
 public:
  BandwidthEstimationHandlerTest() : estimator() {}

 protected:
  void setHandler() {
    picker = std::make_shared<erizo::MockRemoteBitrateEstimatorPicker>();
    EXPECT_CALL(*picker.get(), pickEstimatorProxy(_, _, _))
      .WillRepeatedly(Return(new erizo::RemoteBitrateEstimatorProxy(&estimator)));
    bwe_handler = std::make_shared<BandwidthEstimationHandler>(picker);
    pipeline->addBack(bwe_handler);
  }

  void afterPipelineSetup() {
  }

    std::shared_ptr<erizo::MockMediaStream> addMediaStreamToConnection(std::string id,
      bool is_publisher, bool ready) {
      auto media_stream =
        std::make_shared<erizo::MockMediaStream>(simulated_worker, connection, id, id, rtp_maps, is_publisher);
      std::shared_ptr<erizo::MediaStream> stream_ptr = std::dynamic_pointer_cast<erizo::MediaStream>(media_stream);
      connection->addMediaStream(stream_ptr);
      simulated_worker->executeTasks();
      EXPECT_CALL(*media_stream.get(), isReady()).WillRepeatedly(Return(ready));
      streams.push_back(media_stream);
      return media_stream;
  }

  void internalTearDown() {
    std::for_each(streams.begin(), streams.end(),
      [this](const std::shared_ptr<erizo::MockMediaStream> &stream) {
        connection->removeMediaStream(stream->getId());
      });
    simulated_worker->executeTasks();
  }

  std::shared_ptr<BandwidthEstimationHandler> bwe_handler;
  std::shared_ptr<erizo::MockRemoteBitrateEstimatorPicker> picker;
  erizo::MockRemoteBitrateEstimator estimator;
  std::vector<std::shared_ptr<erizo::MockMediaStream>> streams;
};

TEST_F(BandwidthEstimationHandlerTest, basicBehaviourShouldWritePackets) {
  auto packet1 = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);
  auto packet2 = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(2);
  pipeline->write(packet1);
  pipeline->write(packet2);
}

TEST_F(BandwidthEstimationHandlerTest, basicBehaviourShouldReadPackets) {
  auto packet1 = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);
  auto packet2 = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  EXPECT_CALL(estimator, Process());
  EXPECT_CALL(estimator, TimeUntilNextProcess()).WillRepeatedly(Return(1000));
  EXPECT_CALL(estimator, IncomingPacket(_, _, _));
  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(2);
  pipeline->read(packet1);
  pipeline->read(packet2);
}

TEST_F(BandwidthEstimationHandlerTest, shouldSendRembPacketWithEstimatedBitrateIfThereIsAPublisherReady) {
  uint32_t kArbitraryBitrate = 100000;

  addMediaStreamToConnection("test1", true, true);

  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(estimator, Process());
  EXPECT_CALL(estimator, TimeUntilNextProcess()).WillRepeatedly(Return(1000));
  EXPECT_CALL(estimator, IncomingPacket(_, _, _));
  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);

  EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::RembHasBitrateValue(kArbitraryBitrate))).Times(1);
  picker->observer_->OnReceiveBitrateChanged(std::vector<uint32_t>(), kArbitraryBitrate);
}

TEST_F(BandwidthEstimationHandlerTest, shouldNotSendRembPacketWithEstimatedBitrateIfThePublisherIsNotReady) {
  uint32_t kArbitraryBitrate = 100000;

  addMediaStreamToConnection("test1", true, false);

  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(estimator, Process());
  EXPECT_CALL(estimator, TimeUntilNextProcess()).WillRepeatedly(Return(1000));
  EXPECT_CALL(estimator, IncomingPacket(_, _, _));
  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);

  EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::RembHasBitrateValue(kArbitraryBitrate))).Times(0);
  picker->observer_->OnReceiveBitrateChanged(std::vector<uint32_t>(), kArbitraryBitrate);
}
