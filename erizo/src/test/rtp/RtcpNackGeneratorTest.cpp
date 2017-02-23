#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/RtcpNackGenerator.h>
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
using erizo::RtcpNackGenerator;
using erizo::RtcpHeader;
using erizo::WebRtcConnection;
using erizo::SimulatedClock;

class RtcpNackGeneratorTest :public ::testing::Test {
 public:
  RtcpNackGeneratorTest(): clock{std::make_shared<SimulatedClock>()}, nack_generator{erizo::kVideoSsrc,
    clock} {}

 protected:
  void advanceClockMs(int time_ms) {
    clock->advanceTime(std::chrono::milliseconds(time_ms));
  }

  std::shared_ptr<SimulatedClock> clock;
  RtcpNackGenerator nack_generator;
};

TEST_F(RtcpNackGeneratorTest, shouldNotGenerateNackForConsecutivePackets) {
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + 1,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  EXPECT_FALSE(nack_generator.handleRtpPacket(second_packet));
}

TEST_F(RtcpNackGeneratorTest, shouldGenerateNackOnLostPacket) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  EXPECT_TRUE(nack_generator.handleRtpPacket(second_packet));
}

TEST_F(RtcpNackGeneratorTest, shouldNotRepeatNackIfNoTimeHasPassed) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  auto third_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  EXPECT_TRUE(nack_generator.handleRtpPacket(second_packet));
  EXPECT_FALSE(nack_generator.handleRtpPacket(third_packet));

  std::shared_ptr<dataPacket> receiver_report = erizo::PacketTools::createReceiverReport(erizo::kVideoSsrc,
      erizo::kVideoSsrc, kArbitraryNumberOfPackets, VIDEO_PACKET);
  nack_generator.addNackPacketToRr(receiver_report);
  char* moving_buf = receiver_report->data;
  int rtcp_length = 0;
  int total_length = 0;

  do {
    moving_buf += rtcp_length;
    RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(moving_buf);
    rtcp_length = (ntohs(chead->length) + 1) * 4;
    total_length += rtcp_length;

    if (chead->packettype == RTCP_RTP_Feedback_PT) {
      uint16_t initial_seq_num = chead->getNackPid();
      uint16_t plb = chead->getNackBlp();
      uint16_t kNackBlpSize = 16;
      for (int i = -1; i <= kNackBlpSize; i++) {
        if ((plb >> i)  & 0x0001) {
          uint16_t seq_num = initial_seq_num + i + 1;
        }
      }
    }
  } while (total_length < receiver_report->length);
}

TEST_F(RtcpNackGeneratorTest, shouldRepeatNacks2TimesAtMost) {
  uint16_t kMaxSeqnum = 65534;
  uint16_t kArbitraryNumberOfPackets = 4;
  uint16_t kSeqnumCyclesExpected = 1;
  uint16_t kNewSeqNum = kMaxSeqnum + kArbitraryNumberOfPackets;


  auto first_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum + kArbitraryNumberOfPackets, VIDEO_PACKET);
}

TEST_F(RtcpNackGeneratorTest, shouldFillInBLPCorrectly) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  nack_generator.handleRtpPacket(second_packet);
}
