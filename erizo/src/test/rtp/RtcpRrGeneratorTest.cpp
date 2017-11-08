#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/RtcpRrGenerator.h>
#include <lib/Clock.h>
#include <lib/ClockUtils.h>
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
using erizo::DataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::RtcpRrGenerator;
using erizo::RtcpHeader;
using erizo::WebRtcConnection;
using erizo::SimulatedClock;

class RtcpRrGeneratorTest :public ::testing::Test {
 public:
  RtcpRrGeneratorTest(): clock{std::make_shared<SimulatedClock>()}, rr_generator{erizo::kVideoSsrc,
    VIDEO_PACKET, clock} {}

 protected:
  void advanceClockMs(int time_ms) {
    clock->advanceTime(std::chrono::milliseconds(time_ms));
  }

  std::shared_ptr<SimulatedClock> clock;
  RtcpRrGenerator rr_generator;
};

TEST_F(RtcpRrGeneratorTest, shouldReportPacketLoss) {
  uint16_t kArbitraryPacketsLost = 8;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryPacketsLost + 1,
      VIDEO_PACKET);
  rr_generator.handleRtpPacket(first_packet);
  rr_generator.handleRtpPacket(second_packet);
  std::shared_ptr<DataPacket> rr_packet = rr_generator.generateReceiverReport();
  RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(rr_packet->data);
  EXPECT_EQ(rtcp_header->getLostPackets(), kArbitraryPacketsLost);
}

TEST_F(RtcpRrGeneratorTest, shouldReportFractionLost) {
  uint16_t kArbitraryPacketsLost = 2;
  uint16_t kPercentagePacketLoss = 50;
  uint8_t kFractionLost = kPercentagePacketLoss * 256/100;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryPacketsLost + 1,
      VIDEO_PACKET);
  rr_generator.handleRtpPacket(first_packet);
  rr_generator.handleRtpPacket(second_packet);

  std::shared_ptr<DataPacket> rr_packet = rr_generator.generateReceiverReport();
  RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(rr_packet->data);
  EXPECT_EQ(rtcp_header->getFractionLost(), kFractionLost);
}

TEST_F(RtcpRrGeneratorTest, shouldReportHighestSeqnum) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);

  rr_generator.handleRtpPacket(first_packet);
  rr_generator.handleRtpPacket(second_packet);

  std::shared_ptr<DataPacket> rr_packet = rr_generator.generateReceiverReport();
  RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(rr_packet->data);
  EXPECT_EQ(rtcp_header->getHighestSeqnum(), erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets);
}

TEST_F(RtcpRrGeneratorTest, shouldReportHighestSeqnumWithRollover) {
  uint16_t kMaxSeqnum = 65534;
  uint16_t kArbitraryNumberOfPackets = 4;
  uint16_t kSeqnumCyclesExpected = 1;
  uint16_t kNewSeqNum = kMaxSeqnum + kArbitraryNumberOfPackets;

  auto first_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum + kArbitraryNumberOfPackets, VIDEO_PACKET);

  rr_generator.handleRtpPacket(first_packet);
  rr_generator.handleRtpPacket(second_packet);

  std::shared_ptr<DataPacket> rr_packet = rr_generator.generateReceiverReport();
  RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(rr_packet->data);
  EXPECT_EQ(rtcp_header->getSeqnumCycles(), kSeqnumCyclesExpected);
  EXPECT_EQ(rtcp_header->getHighestSeqnum(), kNewSeqNum);
}


TEST_F(RtcpRrGeneratorTest, shouldReportDelaySinceLastSr) {
  int kArbitraryTimePassedInMs = 500;
  uint kArbitratyTimePassed = kArbitraryTimePassedInMs * 65536/1000;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto sender_report = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET);
  sender_report->received_time_ms = erizo::ClockUtils::timePointToMs(clock->now());
  rr_generator.handleRtpPacket(first_packet);
  rr_generator.handleSr(sender_report);
  advanceClockMs(kArbitraryTimePassedInMs);

  std::shared_ptr<DataPacket> rr_packet = rr_generator.generateReceiverReport();
  RtcpHeader* rtcp_header = reinterpret_cast<RtcpHeader*>(rr_packet->data);
  EXPECT_EQ(rtcp_header->getDelaySinceLastSr(), kArbitratyTimePassed);
}
