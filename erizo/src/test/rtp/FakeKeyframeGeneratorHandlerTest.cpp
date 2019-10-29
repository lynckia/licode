#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/FakeKeyframeGeneratorHandler.h>
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
using ::testing::Not;
using erizo::DataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::MovingIntervalRateStat;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::FakeKeyframeGeneratorHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;


class FakeKeyframeGeneratorHandlerTest : public erizo::HandlerTest {
 public:
  FakeKeyframeGeneratorHandlerTest() {}

 protected:
  void setHandler() {
    keyframe_generator = std::make_shared<FakeKeyframeGeneratorHandler>();
    pipeline->addBack(keyframe_generator);
  }
  std::shared_ptr<FakeKeyframeGeneratorHandler> keyframe_generator;
};

TEST_F(FakeKeyframeGeneratorHandlerTest, basicBehaviourShouldReadPackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);
}

TEST_F(FakeKeyframeGeneratorHandlerTest, basicBehaviourShouldWritePackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->write(packet);
}

TEST_F(FakeKeyframeGeneratorHandlerTest, shouldNotChangePacketsWhenDisabled) {
  keyframe_generator->disable();

  auto packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, false, false);
  int length = packet->length;
  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::PacketLengthIs(length))).Times(1);
  pipeline->write(packet);
}

TEST_F(FakeKeyframeGeneratorHandlerTest, shouldTransformVP8Packet) {
  keyframe_generator->enable();
  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::PacketIsKeyframe())).Times(1);
  pipeline->write(erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, false, false));
}

TEST_F(FakeKeyframeGeneratorHandlerTest, shouldNotTransformVP8Keyframes) {
  auto packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
  int length = packet->length;
  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::PacketLengthIs(length))).Times(1);
  pipeline->write(packet);
}

TEST_F(FakeKeyframeGeneratorHandlerTest, shouldNotTransformUnsupportedCodecPackets) {
  auto packet = erizo::PacketTools::createVP9Packet(erizo::kArbitrarySeqNumber, false, false);
  int length = packet->length;
  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::PacketLengthIs(length))).Times(1);
  pipeline->write(packet);
}

TEST_F(FakeKeyframeGeneratorHandlerTest, shouldSendPLIInmediatelyIfNoKeyframeIsReceived) {
  auto packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, false, false);
  EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(erizo::IsPLI())).Times(1);
  pipeline->write(packet);
}

TEST_F(FakeKeyframeGeneratorHandlerTest, shouldNotSendPLIIfKeyframeIsFirstPacket) {
  auto packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
  EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(erizo::IsPLI())).Times(0);
  pipeline->write(packet);
}

TEST_F(FakeKeyframeGeneratorHandlerTest, shouldSendPLIsPeriodically) {
  auto packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, false, false);
  EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(erizo::IsPLI())).Times(2);
  pipeline->write(packet);
  executeTasksInNextMs(350);
}

TEST_F(FakeKeyframeGeneratorHandlerTest, shouldNotSendPeriodicPLISIfKeyframeIsReceived) {
  auto packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, false, false);
  auto keyframe_packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
  EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(erizo::IsPLI())).Times(1);
  pipeline->write(packet);
  pipeline->write(keyframe_packet);
  executeTasksInNextMs(350);
}

