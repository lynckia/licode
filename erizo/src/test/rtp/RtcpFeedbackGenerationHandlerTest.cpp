#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/RtcpFeedbackGenerationHandler.h>
#include <lib/Clock.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

#include <string>
#include <vector>

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using ::testing::AllOf;
using erizo::dataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::RtcpFeedbackGenerationHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using erizo::SimulatedClock;

class RtcpFeedbackRrGenerationTest : public erizo::HandlerTest {
 public:
  RtcpFeedbackRrGenerationTest(): clock{std::make_shared<SimulatedClock>()} {}

 protected:
  void setHandler() {
    rr_handler = std::make_shared<RtcpFeedbackGenerationHandler>(false, clock);
    pipeline->addBack(rr_handler);
  }

  void advanceClockMs(int time_ms) {
    clock->advanceTime(std::chrono::milliseconds(time_ms));
  }

  std::shared_ptr<RtcpFeedbackGenerationHandler> rr_handler;
  std::shared_ptr<SimulatedClock> clock;
};

TEST_F(RtcpFeedbackRrGenerationTest, basicBehaviourShouldReadPackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);
}

TEST_F(RtcpFeedbackRrGenerationTest, basicBehaviourShouldWritePackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->write(packet);
}


TEST_F(RtcpFeedbackRrGenerationTest, shouldReportHighestSeqnum) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  auto sender_report = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET);
  EXPECT_CALL(*reader.get(), read(_, _)).Times(2);
  EXPECT_CALL(*writer.get(), write(_, _))
    .With(Args<1>(erizo::ReceiverReportHasSequenceNumber(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets)))
    .Times(1);

  pipeline->read(first_packet);
  advanceClockMs(2000);
  pipeline->read(second_packet);
}

