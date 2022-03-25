#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpPaddingManagerHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>
#include <stats/StatNode.h>
#include <Stats.h>

#include <queue>
#include <string>
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
using erizo::MovingIntervalRateStat;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::RtpPaddingManagerHandler;
using erizo::PaddingManagerMode;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::CumulativeStat;
using erizo::Worker;
using std::queue;
using erizo::MediaStream;



class RtpPaddingManagerHandlerBaseTest : public erizo::BaseHandlerTest {
 public:
  RtpPaddingManagerHandlerBaseTest() {}

 protected:
  void internalSetHandler() {
    clock = std::make_shared<erizo::SimulatedClock>();
    padding_manager_handler = std::make_shared<RtpPaddingManagerHandler>(clock);
    pipeline->addBack(padding_manager_handler);
  }

  void whenSubscribersWithTargetBitrate(std::vector<uint32_t> subscriber_bitrates) {
    int i = 0;
    std::for_each(subscriber_bitrates.begin(), subscriber_bitrates.end(), [this, &i](uint32_t bitrate) {
      addMediaStreamToConnection("sub" + std::to_string(i), false, bitrate);
      simulated_worker->executeTasks();
      i++;
    });
  }

  void whenPublishers(uint num_publishers) {
    for (uint i = 0; i < num_publishers; i++) {
      addMediaStreamToConnection("pub" + std::to_string(i), true, 0);
      simulated_worker->executeTasks();
    }
  }

  void whenBandwidthEstimationIs(uint32_t bitrate) {
    stats->getNode()["total"].insertStat("senderBitrateEstimation", CumulativeStat{bitrate});
  }

  void whenCurrentTotalVideoBitrateIs(uint32_t bitrate) {
    stats->getNode()["total"].insertStat("videoBitrate", CumulativeStat{bitrate});
  }

  void internalTearDown() {
    std::for_each(subscribers.begin(), subscribers.end(),
      [this](const std::shared_ptr<erizo::MockMediaStream> &stream) {
        connection->removeMediaStream(stream->getId());
      });
    std::for_each(publishers.begin(), publishers.end(),
      [this](const std::shared_ptr<erizo::MockMediaStream> &stream) {
        connection->removeMediaStream(stream->getId());
      });
    simulated_worker->executeTasks();
  }

  std::shared_ptr<erizo::MockMediaStream> addMediaStreamToConnection(std::string id,
      bool is_publisher, uint32_t bitrate) {
    auto media_stream =
      std::make_shared<erizo::MockMediaStream>(simulated_worker, connection, id, id, rtp_maps, is_publisher);
    std::shared_ptr<erizo::MediaStream> stream_ptr = std::dynamic_pointer_cast<erizo::MediaStream>(media_stream);
    connection->addMediaStream(stream_ptr);
    EXPECT_CALL(*media_stream.get(), getTargetVideoBitrate()).WillRepeatedly(Return(bitrate));
    if (is_publisher) {
      publishers.push_back(media_stream);
    } else {
      subscribers.push_back(media_stream);
    }

    return media_stream;
  }

  void expectPaddingBitrate(uint64_t bitrate, int times = 1) {
    std::for_each(subscribers.begin(), subscribers.end(),
      [bitrate, times](const std::shared_ptr<erizo::MockMediaStream> &stream) {
        EXPECT_CALL(*stream.get(), setTargetPaddingBitrate(testing::Eq(bitrate))).Times(times);
      });

    std::for_each(publishers.begin(), publishers.end(),
      [bitrate](const std::shared_ptr<erizo::MockMediaStream> &stream) {
        EXPECT_CALL(*stream.get(), setTargetPaddingBitrate(_)).Times(0);
      });
  }

  std::vector<std::shared_ptr<erizo::MockMediaStream>> subscribers;
  std::vector<std::shared_ptr<erizo::MockMediaStream>> publishers;
  std::shared_ptr<RtpPaddingManagerHandler> padding_manager_handler;
  std::shared_ptr<erizo::SimulatedClock> clock;
};

class RtpPaddingManagerHandlerTest : public ::testing::Test, public RtpPaddingManagerHandlerBaseTest {
 public:
  RtpPaddingManagerHandlerTest() {}

  void setHandler() override {
    internalSetHandler();
  }

 protected:
  virtual void SetUp() {
    internalSetUp();
  }

  void TearDown() override {
    internalTearDown();
  }
};

TEST_F(RtpPaddingManagerHandlerTest, basicBehaviourShouldReadPackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);
}

TEST_F(RtpPaddingManagerHandlerTest, basicBehaviourShouldWritePackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->write(packet);
}

TEST_F(RtpPaddingManagerHandlerTest, shouldDistributePaddingEvenlyAmongStreamsWithoutPublishers) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);
  //  Stable mode
  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInStartMode + std::chrono::milliseconds(1));
  whenSubscribersWithTargetBitrate({200, 200, 200, 200, 200});
  whenPublishers(0);
  whenBandwidthEstimationIs(600);
  whenCurrentTotalVideoBitrateIs(100);

  expectPaddingBitrate(110);

  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
}

TEST_F(RtpPaddingManagerHandlerTest, shouldGoToHoldModeWhenRembSharplyGoesDown) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInStartMode + std::chrono::milliseconds(1));
  whenSubscribersWithTargetBitrate({500});
  whenPublishers(0);
  whenBandwidthEstimationIs(300);
  whenCurrentTotalVideoBitrateIs(100);

  expectPaddingBitrate(220);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);

  whenBandwidthEstimationIs(100);
  whenCurrentTotalVideoBitrateIs(50);
  expectPaddingBitrate(0);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::HOLD);
}

TEST_F(RtpPaddingManagerHandlerTest, shouldNotGoToHoldModeWhenRembGoesSlightlyDown) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInStartMode + std::chrono::milliseconds(1));
  whenSubscribersWithTargetBitrate({500});
  whenPublishers(0);
  whenBandwidthEstimationIs(300);
  whenCurrentTotalVideoBitrateIs(100);

  expectPaddingBitrate(220);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);

  whenBandwidthEstimationIs(280);
  whenCurrentTotalVideoBitrateIs(100);
  expectPaddingBitrate(198);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::STABLE);
}

TEST_F(RtpPaddingManagerHandlerTest, shouldGoToHoldModeWhenRembSharplyGoesDownInStartMode) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  const uint64_t subscribers_target_bitrate = 500;
  const uint64_t bandwidth_estimation = 300;
  const uint64_t total_video_bitrate = 100;
  whenSubscribersWithTargetBitrate({subscribers_target_bitrate});
  whenPublishers(0);
  whenBandwidthEstimationIs(bandwidth_estimation);
  whenCurrentTotalVideoBitrateIs(total_video_bitrate);

  const float expected_padding = subscribers_target_bitrate - total_video_bitrate;
  expectPaddingBitrate(expected_padding);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::START);

  whenBandwidthEstimationIs(100);
  whenCurrentTotalVideoBitrateIs(50);
  expectPaddingBitrate(0);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::HOLD);
}


TEST_F(RtpPaddingManagerHandlerTest, shouldStartInStartModeAndSendMoreAggressivePadding) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  const uint64_t subscribers_target_bitrate = 1500;
  const uint64_t bandwidth_estimation = 300;
  const uint64_t total_video_bitrate = 100;
  whenSubscribersWithTargetBitrate({subscribers_target_bitrate});
  whenPublishers(0);
  whenBandwidthEstimationIs(bandwidth_estimation);
  whenCurrentTotalVideoBitrateIs(total_video_bitrate);

  const float expected_padding =
    bandwidth_estimation * RtpPaddingManagerHandler::kStartModeFactor - total_video_bitrate;
  expectPaddingBitrate(expected_padding);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);

  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::START);
}

TEST_F(RtpPaddingManagerHandlerTest, shouldNeverGoOverTargetBitrateInStartMode) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  const uint64_t subscribers_target_bitrate = 500;
  const uint64_t bandwidth_estimation = 300;
  const uint64_t total_video_bitrate = 100;
  whenSubscribersWithTargetBitrate({subscribers_target_bitrate});
  whenPublishers(0);
  whenBandwidthEstimationIs(bandwidth_estimation);
  whenCurrentTotalVideoBitrateIs(total_video_bitrate);

  const float expected_padding = subscribers_target_bitrate - total_video_bitrate;
  expectPaddingBitrate(expected_padding);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);

  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::START);
}

TEST_F(RtpPaddingManagerHandlerTest, shouldGoToRecoverModeAfterHoldIfBweIsConsistent) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInStartMode + std::chrono::milliseconds(1));
  whenSubscribersWithTargetBitrate({500});
  whenPublishers(0);
  whenBandwidthEstimationIs(300);
  whenCurrentTotalVideoBitrateIs(100);

  expectPaddingBitrate(220);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);

  whenBandwidthEstimationIs(100);
  whenCurrentTotalVideoBitrateIs(50);
  expectPaddingBitrate(0);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::HOLD);
  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInHoldMode + std::chrono::milliseconds(1));
  pipeline->write(packet);
  expectPaddingBitrate(205);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::RECOVER);
}

TEST_F(RtpPaddingManagerHandlerTest, shouldNotGoToRecoveryModeAfterAFailedAttempt) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInStartMode + std::chrono::milliseconds(1));
  whenSubscribersWithTargetBitrate({500});
  whenPublishers(0);
  whenBandwidthEstimationIs(300);
  whenCurrentTotalVideoBitrateIs(100);

  expectPaddingBitrate(220);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::STABLE);

  whenBandwidthEstimationIs(100);
  whenCurrentTotalVideoBitrateIs(50);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::HOLD);


  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInHoldMode + std::chrono::milliseconds(1));
  pipeline->write(packet);
  expectPaddingBitrate(205);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::RECOVER);


  whenBandwidthEstimationIs(90);
  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(packet);
  expectPaddingBitrate(0, 2);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::HOLD);

  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInHoldMode + std::chrono::milliseconds(1));
  pipeline->write(packet);
  expectPaddingBitrate(44);
  EXPECT_EQ(padding_manager_handler->getCurrentPaddingMode(), PaddingManagerMode::STABLE);
}

typedef std::vector<uint32_t> SubscriberBitratesList;

class RtpPaddingManagerHandlerTestWithParam : public RtpPaddingManagerHandlerBaseTest,
  public ::testing::TestWithParam<std::tr1::tuple<SubscriberBitratesList, uint32_t, uint32_t, uint64_t>> {
 public:
  RtpPaddingManagerHandlerTestWithParam() {
    subscribers = std::tr1::get<0>(GetParam());
    bw_estimation = std::tr1::get<1>(GetParam());
    video_bitrate = std::tr1::get<2>(GetParam());
    expected_padding_bitrate = std::tr1::get<3>(GetParam());
  }

 protected:
  void setHandler() override {
    internalSetHandler();
  }

  virtual void SetUp() {
    internalSetUp();
  }

  void TearDown() override {
    internalTearDown();
  }

  SubscriberBitratesList subscribers;
  uint32_t bw_estimation;
  uint32_t video_bitrate;
  uint64_t expected_padding_bitrate;
};

TEST_P(RtpPaddingManagerHandlerTestWithParam, shouldDistributePaddingWithPublishersInStableMode) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  whenSubscribersWithTargetBitrate(subscribers);
  whenPublishers(10);
  whenBandwidthEstimationIs(bw_estimation);
  whenCurrentTotalVideoBitrateIs(video_bitrate);

  expectPaddingBitrate(expected_padding_bitrate);

  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInStartMode + std::chrono::milliseconds(1));
  pipeline->write(packet);
}

TEST_P(RtpPaddingManagerHandlerTestWithParam, shouldDistributePaddingWithNoPublishersInStableMode) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  whenSubscribersWithTargetBitrate(subscribers);
  whenPublishers(0);
  whenBandwidthEstimationIs(bw_estimation);
  whenCurrentTotalVideoBitrateIs(video_bitrate);

  expectPaddingBitrate(expected_padding_bitrate);

  clock->advanceTime(RtpPaddingManagerHandler::kMaxDurationInStartMode + std::chrono::milliseconds(1));
  pipeline->write(packet);
}

INSTANTIATE_TEST_CASE_P(
  Padding_values, RtpPaddingManagerHandlerTestWithParam, testing::Values(
    //                                          targetBitrates,       bwe, bitrate, expectedPaddingBitrate
    std::make_tuple(SubscriberBitratesList{200, 200, 200, 200, 200},  600,     100,
                                                  100*RtpPaddingManagerHandler::kStableModeAvailableFactor),
    std::make_tuple(SubscriberBitratesList{200, 200, 200, 200, 200}, 1500,     100,                      0),
    std::make_tuple(SubscriberBitratesList{200, 200, 200, 200, 200},   99,     100,                      0),
    std::make_tuple(SubscriberBitratesList{200, 200, 200, 200, 200},  600,     600,                      0),
    std::make_tuple(SubscriberBitratesList{200, 200, 200, 200, 200},    0,     100,                      0),
    std::make_tuple(SubscriberBitratesList{200, 200, 200, 200, 200}, 1000,       0,                    200)));
