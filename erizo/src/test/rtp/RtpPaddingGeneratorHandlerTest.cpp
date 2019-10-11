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
    clock = std::make_shared<erizo::SimulatedClock>();
    padding_generator_handler = std::make_shared<RtpPaddingGeneratorHandler>(clock);
    pipeline->addBack(padding_generator_handler);
    EXPECT_CALL(*media_stream.get(), getTargetPaddingBitrate()).WillRepeatedly(Return(0));
    EXPECT_CALL(*media_stream.get(), isSlideShowModeEnabled()).WillRepeatedly(Return(false));
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

  EXPECT_CALL(*media_stream.get(), getTargetPaddingBitrate()).WillRepeatedly(Return(uint64_t(10000)));
  pipeline->notifyUpdate();

  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true));

  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, true));
}

TEST_F(RtpPaddingGeneratorHandlerTest, shouldNotSendPaddingWhenDisabled) {
  EXPECT_CALL(*writer.get(), write(_, _)).Times(2);
  EXPECT_CALL(*media_stream.get(), getTargetPaddingBitrate()).WillRepeatedly(Return(0));
  pipeline->notifyUpdate();

  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true));

  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, true));
}

TEST_F(RtpPaddingGeneratorHandlerTest, shouldNotSendPaddingAfterNotMarkers) {
  EXPECT_CALL(*writer.get(), write(_, _)).Times(2);
  EXPECT_CALL(*media_stream.get(), getTargetPaddingBitrate()).WillRepeatedly(Return(uint64_t(10000)));
  pipeline->notifyUpdate();

  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, false));

  clock->advanceTime(std::chrono::milliseconds(200));
  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, false));
}
