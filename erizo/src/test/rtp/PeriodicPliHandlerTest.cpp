
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <rtp/PeriodicPliHandler.h>
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
using erizo::PeriodicPliHandler;
using erizo::SimulatedClock;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;

constexpr int kArbitraryKeyframePeriodMs = 1000;


class PeriodicPliHandlerTest : public erizo::HandlerTest {
 public:
  PeriodicPliHandlerTest() {}

 protected:
  void setHandler() {
    periodic_pli_handler = std::make_shared<PeriodicPliHandler>(simulated_clock);
    pipeline->addBack(periodic_pli_handler);
  }

  std::shared_ptr<PeriodicPliHandler> periodic_pli_handler;
};


TEST_F(PeriodicPliHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(PeriodicPliHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(PeriodicPliHandlerTest, shouldSendPliEveryInterval) {
    periodic_pli_handler->updateInterval(true, kArbitraryKeyframePeriodMs);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(2);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs+1);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs+1);
}

TEST_F(PeriodicPliHandlerTest, shouldNotSendPliIfDeactivated) {
    periodic_pli_handler->updateInterval(false, 0);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(0);

    executeTasksInNextMs(kArbitraryKeyframePeriodMs+1);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs+1);
}

TEST_F(PeriodicPliHandlerTest, shouldUpdateIntervalIfRequested) {
    periodic_pli_handler->updateInterval(true, kArbitraryKeyframePeriodMs);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(2);

    executeTasksInNextMs(kArbitraryKeyframePeriodMs+1);
    periodic_pli_handler->updateInterval(true, 2*kArbitraryKeyframePeriodMs);
    executeTasksInNextMs(2*kArbitraryKeyframePeriodMs+1);
}

TEST_F(PeriodicPliHandlerTest, shouldNotSendPliIfMoreThanOneKeyframeFromTheLowestLayerIsReceivedInPeriod) {
    auto keyframe = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
    keyframe->compatible_spatial_layers.push_back(0);
    auto keyframe2 = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, true);
    keyframe2->compatible_spatial_layers.push_back(0);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(0);
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    periodic_pli_handler->updateInterval(true, kArbitraryKeyframePeriodMs);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs/2);
    pipeline->read(keyframe);
    pipeline->read(keyframe2);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs/2 + 1);
}

TEST_F(PeriodicPliHandlerTest, shouldSendPliIfNoKeyframesFromLowestLayerAreReceivedInPeriod) {
    auto keyframe = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
    keyframe->compatible_spatial_layers.push_back(1);
    auto keyframe2 = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, true, true);
    keyframe2->compatible_spatial_layers.push_back(2);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(1);
    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    periodic_pli_handler->updateInterval(true, kArbitraryKeyframePeriodMs);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs/2);
    pipeline->read(keyframe);
    pipeline->read(keyframe2);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs/2 + 1);
}

TEST_F(PeriodicPliHandlerTest, shouldSendPliWhenRequestedToStop) {
    auto keyframe = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(1);

    periodic_pli_handler->updateInterval(true, kArbitraryKeyframePeriodMs);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs/3);
    periodic_pli_handler->updateInterval(false, 0);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs*2 + 1);
}

TEST_F(PeriodicPliHandlerTest, shouldNotSchedulePlisWhenDisabled) {
    periodic_pli_handler->disable();

    periodic_pli_handler->updateInterval(true, kArbitraryKeyframePeriodMs);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(erizo::IsPLI())).Times(0);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs+1);
    executeTasksInNextMs(kArbitraryKeyframePeriodMs+1);
}
