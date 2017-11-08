#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <rtp/PliPacerHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

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
using erizo::DataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::PliPacerHandler;
using erizo::SimulatedClock;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;

constexpr int kShortPeriodMs =
  std::chrono::duration_cast<std::chrono::milliseconds>(PliPacerHandler::kMinPLIPeriod).count() / 5;

constexpr int kPLIPeriodMs =
  std::chrono::duration_cast<std::chrono::milliseconds>(PliPacerHandler::kMinPLIPeriod).count();

constexpr int kKeyframeTimeoutMs =
  std::chrono::duration_cast<std::chrono::milliseconds>(PliPacerHandler::kKeyframeTimeout).count();


class PliPacerHandlerTest : public erizo::HandlerTest {
 public:
  PliPacerHandlerTest() {}

 protected:
  void setHandler() {
    pli_pacer_handler = std::make_shared<PliPacerHandler>(simulated_clock);
    pipeline->addBack(pli_pacer_handler);
  }

  std::shared_ptr<PliPacerHandler> pli_pacer_handler;
};

TEST_F(PliPacerHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(PliPacerHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(PliPacerHandlerTest, shouldSendASinglePLIWhenReceivingSeveralInAShortPeriod) {
    auto pli1 = erizo::PacketTools::createPLI();
    auto pli2 = erizo::PacketTools::createPLI();
    auto pli3 = erizo::PacketTools::createPLI();

    EXPECT_CALL(*writer.get(), write(_, _)).Times(1);
    pipeline->write(pli1);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli2);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli3);
    executeTasksInNextMs(kShortPeriodMs);
}

TEST_F(PliPacerHandlerTest, shouldResetPeriodWhenKeyframeIsReceived) {
    auto pli1 = erizo::PacketTools::createPLI();
    auto keyframe = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
    auto pli2 = erizo::PacketTools::createPLI();

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(2);
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->write(pli1);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->read(keyframe);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli2);
    executeTasksInNextMs(kShortPeriodMs);
}

TEST_F(PliPacerHandlerTest, shouldSendMultiplePLIsWhenPeriodIsExpiredWithNoKeyframes) {
    auto pli1 = erizo::PacketTools::createPLI();
    auto keyframe = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(2);
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->read(keyframe);
    pipeline->write(pli1);

    executeTasksInNextMs(kPLIPeriodMs + 1);
}

TEST_F(PliPacerHandlerTest, shouldNotSendMultiplePLIsWhenDisabled) {
    auto pli1 = erizo::PacketTools::createPLI();
    auto keyframe = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
    pli_pacer_handler->disable();

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(1);
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->read(keyframe);
    pipeline->write(pli1);

    executeTasksInNextMs(kPLIPeriodMs + 1);
}

TEST_F(PliPacerHandlerTest, shouldSendFIRWhenKeyframesAreNotReceivedInALongPeriod) {
    auto pli1 = erizo::PacketTools::createPLI();

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(50);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsFIR())).Times(3);
    pipeline->write(pli1);

    executeTasksInNextMs(kKeyframeTimeoutMs + 10);
}
