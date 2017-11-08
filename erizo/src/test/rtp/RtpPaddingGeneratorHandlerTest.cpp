#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpPaddingGeneratorHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>
#include <stats/StatNode.h>

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
using erizo::RtpPaddingGeneratorHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;


class RtpPaddingGeneratorHandlerTest : public erizo::HandlerTest {
 public:
  RtpPaddingGeneratorHandlerTest() {}

 protected:
  void setHandler() {
    EXPECT_CALL(*processor.get(), getMaxVideoBW()).Times(AtLeast(0));
    EXPECT_CALL(*quality_manager.get(), isPaddingEnabled()).Times(AtLeast(0));
    clock = std::make_shared<erizo::SimulatedClock>();
    padding_generator_handler = std::make_shared<RtpPaddingGeneratorHandler>(clock);
    pipeline->addBack(padding_generator_handler);

    stats->getNode()[erizo::kVideoSsrc].insertStat("bitrateCalculated",
            MovingIntervalRateStat{std::chrono::milliseconds(100), 10, 1., clock});
    stats->getNode()["total"].insertStat("senderBitrateEstimation",
            MovingIntervalRateStat{std::chrono::milliseconds(100), 10, 1., clock});

    EXPECT_CALL(*quality_manager.get(), isPaddingEnabled()).WillRepeatedly(Return(true));

    EXPECT_CALL(*processor.get(), getMaxVideoBW()).WillRepeatedly(Return(100000));
    stats->getNode()[erizo::kVideoSsrc]["bitrateCalculated"]       += 40000 * 2 / 10;
    stats->getNode()["total"]["senderBitrateEstimation"] += 60000 * 2 / 10;
  }

  std::shared_ptr<RtpPaddingGeneratorHandler> padding_generator_handler;
  std::shared_ptr<erizo::SimulatedClock> clock;
};

TEST_F(RtpPaddingGeneratorHandlerTest, basicBehaviourShouldReadPackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);
}

TEST_F(RtpPaddingGeneratorHandlerTest, basicBehaviourShouldWritePackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->write(packet);
}

TEST_F(RtpPaddingGeneratorHandlerTest, shouldSendPaddingWhenEnabled) {
  EXPECT_CALL(*writer.get(), write(_, _)).Times(AtLeast(3));

  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true));

  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, true));
}

TEST_F(RtpPaddingGeneratorHandlerTest, shouldNotSendPaddingWhenDisabled) {
  EXPECT_CALL(*writer.get(), write(_, _)).Times(2);
  EXPECT_CALL(*quality_manager.get(), isPaddingEnabled()).WillRepeatedly(Return(false));
  pipeline->notifyUpdate();

  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true));

  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, true));
}

TEST_F(RtpPaddingGeneratorHandlerTest, shouldNotSendPaddingAfterNotMarkers) {
  EXPECT_CALL(*writer.get(), write(_, _)).Times(2);

  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true));

  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, false));
}

TEST_F(RtpPaddingGeneratorHandlerTest, shouldNotSendPaddingIfBitrateIsHigherThanBitrateEstimation) {
  const uint32_t kFractionLost = .1 * 255;
  EXPECT_CALL(*writer.get(), write(_, _)).Times(2);

  stats->getNode()[erizo::kVideoSsrc]["bitrateCalculated"] += 70000;
  stats->getNode()["total"]["senderBitrateEstimation"]     += 60000;

  pipeline->read(erizo::PacketTools::createReceiverReport(erizo::kAudioSsrc, erizo::kAudioSsrc,
                                                          erizo::kArbitrarySeqNumber, VIDEO_PACKET, 0, kFractionLost));

  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true));

  clock->advanceTime(std::chrono::milliseconds(1000));

  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, true));
}
