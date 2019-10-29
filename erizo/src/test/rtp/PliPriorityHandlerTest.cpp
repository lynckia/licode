
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <rtp/PliPriorityHandler.h>
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
using erizo::PliPriorityHandler;
using erizo::SimulatedClock;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;

constexpr int kShortPeriodMs =
  std::chrono::duration_cast<std::chrono::milliseconds>(PliPriorityHandler::kLowPriorityPliPeriod).count() / 3;

constexpr int kLowPriorityPliPeriodMs =
  std::chrono::duration_cast<std::chrono::milliseconds>(PliPriorityHandler::kLowPriorityPliPeriod).count();

class PliPriorityHandlerTest : public erizo::HandlerTest {
 public:
  PliPriorityHandlerTest() {}

 protected:
  void setHandler() {
    pli_priority_handler = std::make_shared<PliPriorityHandler>(simulated_clock);
    pipeline->addBack(pli_priority_handler);
  }

  std::shared_ptr<PliPriorityHandler> pli_priority_handler;
};

TEST_F(PliPriorityHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(PliPriorityHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(PliPriorityHandlerTest, shouldSendTheFirstPLIInmediately) {
    auto pli1 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(1);
    pipeline->write(pli1);
}

TEST_F(PliPriorityHandlerTest, shouldSendASinglePLIWhenReceivingSeveralInAShortPeriod) {
    auto pli1 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);
    auto pli2 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);
    auto pli3 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);
    auto pli4 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(2);
    pipeline->write(pli1);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli2);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli3);
    pipeline->write(pli4);
    executeTasksInNextMs(kShortPeriodMs);
}

TEST_F(PliPriorityHandlerTest, shouldNotSendIfKeyframeIsReceivedInPeriod) {
    auto pli1 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);
    auto keyframe = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
    auto pli2 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);
    auto pli3 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(1);
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->write(pli1);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli2);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli3);
    pipeline->read(keyframe);
    executeTasksInNextMs(kShortPeriodMs + 1);
}

TEST_F(PliPriorityHandlerTest, shouldNotStopHighPriorityPlis) {
    auto pli1 = erizo::PacketTools::createPLI(erizo::HIGH_PRIORITY);
    auto pli2 = erizo::PacketTools::createPLI(erizo::HIGH_PRIORITY);
    auto pli3 = erizo::PacketTools::createPLI(erizo::HIGH_PRIORITY);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(3);
    pipeline->write(pli1);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli2);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli3);
    executeTasksInNextMs(kShortPeriodMs);
}

TEST_F(PliPriorityHandlerTest, shouldAlwaysSendPLIsWhenDisabled) {
    pli_priority_handler->disable();
    auto pli1 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);
    auto pli2 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);
    auto pli3 = erizo::PacketTools::createPLI(erizo::LOW_PRIORITY);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(3);
    pipeline->write(pli1);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli2);
    executeTasksInNextMs(kShortPeriodMs);
    pipeline->write(pli3);
    executeTasksInNextMs(kShortPeriodMs);
}
