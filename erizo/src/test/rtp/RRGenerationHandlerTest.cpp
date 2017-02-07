#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/RRGenerationHandler.h>
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
using erizo::RRGenerationHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;

class RRGenerationHandlerTest : public erizo::HandlerTest {
 public:
  RRGenerationHandlerTest() {}

 protected:
  void setHandler() {
    rr_handler = std::make_shared<RRGenerationHandler>(false);
    pipeline->addBack(rr_handler);
  }

  std::shared_ptr<RRGenerationHandler> rr_handler;
};

TEST_F(RRGenerationHandlerTest, basicBehaviourShouldReadPackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);
}

TEST_F(RRGenerationHandlerTest, basicBehaviourShouldWritePackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->write(packet);
}

TEST_F(RRGenerationHandlerTest, shouldReportPacketLoss) {
  uint16_t kArbitraryPacketsLost = 8;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryPacketsLost + 1,
      VIDEO_PACKET);
  auto sender_report = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).Times(3);
  EXPECT_CALL(*writer.get(), write(_, _))
    .With(Args<1>(erizo::ReceiverReportHasLostPacketsValue(kArbitraryPacketsLost)))
    .Times(1);

  pipeline->read(first_packet);
  pipeline->read(second_packet);
  pipeline->read(sender_report);
}

TEST_F(RRGenerationHandlerTest, shouldReportFractionLost) {
  uint16_t kArbitraryPacketsLost = 2;
  uint16_t kPercentagePacketLoss = 50;
  uint8_t kFractionLost = kPercentagePacketLoss * 256/100;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryPacketsLost + 1,
      VIDEO_PACKET);
  auto sender_report = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).Times(3);
  EXPECT_CALL(*writer.get(), write(_, _))
    .With(Args<1>(erizo::ReceiverReportHasFractionLostValue(kFractionLost)))
    .Times(1);

  pipeline->read(first_packet);
  pipeline->read(second_packet);
  pipeline->read(sender_report);
}

TEST_F(RRGenerationHandlerTest, shouldReportHighestSeqnum) {
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto sender_report = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).Times(2);
  EXPECT_CALL(*writer.get(), write(_, _))
    .With(Args<1>(erizo::ReceiverReportHasSequenceNumber(erizo::kArbitrarySeqNumber)))
    .Times(1);

  pipeline->read(first_packet);
  pipeline->read(sender_report);
}

TEST_F(RRGenerationHandlerTest, shouldReportHighestSeqnumWithRollover) {
  uint16_t kMaxSeqnum = 65534;
  uint16_t kArbitraryNumberOfPackets = 4;
  uint16_t kSeqnumCyclesExpected = 1;
  uint16_t kNewSeqNum = kMaxSeqnum + kArbitraryNumberOfPackets;

  auto first_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum + kArbitraryNumberOfPackets, VIDEO_PACKET);
  auto sender_report = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).Times(3);
  EXPECT_CALL(*writer.get(), write(_, _))
    .With(Args<1>(AllOf(erizo::ReceiverReportHasSeqnumCycles(kSeqnumCyclesExpected),
            erizo::ReceiverReportHasSequenceNumber(kNewSeqNum))))
    .Times(1);

  pipeline->read(first_packet);
  pipeline->read(second_packet);
  pipeline->read(sender_report);
}
