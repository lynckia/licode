#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtcpNackGenerator.h>
#include <rtp/RtpUtils.h>
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
using erizo::DataPacket;
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

  std::shared_ptr<DataPacket> generateRrWithNack() {
    std::shared_ptr<DataPacket> new_receiver_report = erizo::PacketTools::createReceiverReport(erizo::kVideoSsrc,
        erizo::kVideoSsrc, 0, VIDEO_PACKET);
    nack_generator.addNackPacketToRr(new_receiver_report);
    return new_receiver_report;
  }

  bool RtcpPacketContainsNackSeqNum(std::shared_ptr<DataPacket> rtcp_packet, uint16_t lost_seq_num) {
    char* moving_buf = rtcp_packet->data;
    int rtcp_length = 0;
    int total_length = 0;
    bool found_nack = false;
    do {
      moving_buf += rtcp_length;
      RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(moving_buf);
      rtcp_length = (ntohs(chead->length) + 1) * 4;
      total_length += rtcp_length;

      if (chead->packettype == RTCP_RTP_Feedback_PT) {
        erizo::RtpUtils::forEachNack(chead, [lost_seq_num, &found_nack](uint16_t seq_num,
           uint16_t plb, RtcpHeader* nack_head) {
          uint16_t initial_seq_num = seq_num;
          if (initial_seq_num == lost_seq_num) {
             found_nack = true;
             return;
          }
          uint16_t kNackBlpSize = 16;
          for (int i = -1; i <= kNackBlpSize; i++) {
            if ((plb >> i)  & 0x0001) {
              uint16_t seq_num = initial_seq_num + i + 1;
              if (seq_num == lost_seq_num) {
                found_nack = true;
                return;
              }
            }
          }
        });
      }
    } while (total_length < rtcp_packet->length);
    return found_nack;
  }

  std::shared_ptr<SimulatedClock> clock;
  std::shared_ptr<DataPacket> receiver_report;
  RtcpNackGenerator nack_generator;
  const uint16_t kMaxSeqnum = 65534;
};

TEST_F(RtcpNackGeneratorTest, shouldNotGenerateNackForConsecutivePackets) {
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + 1,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  EXPECT_FALSE(nack_generator.handleRtpPacket(second_packet));
}

TEST_F(RtcpNackGeneratorTest, shouldNotGenerateNackForConsecutivePacketsWithRollOver) {
  auto first_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum + 1,
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

TEST_F(RtcpNackGeneratorTest, shouldGenerateNackOnLostPacketWithRollOver) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(kMaxSeqnum + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  EXPECT_TRUE(nack_generator.handleRtpPacket(second_packet));
}

TEST_F(RtcpNackGeneratorTest, nackShouldContainLostPackets) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);

  EXPECT_TRUE(nack_generator.handleRtpPacket(second_packet));

  receiver_report = generateRrWithNack();
  EXPECT_TRUE(RtcpPacketContainsNackSeqNum(receiver_report, erizo::kArbitrarySeqNumber + 1));
}

TEST_F(RtcpNackGeneratorTest, nackShouldContainLostPacketsInMoreThanOneBlock) {
  uint16_t kArbitraryNumberOfPackets = 30;  // Bigger than 16 - one block
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);

  EXPECT_TRUE(nack_generator.handleRtpPacket(second_packet));

  receiver_report = generateRrWithNack();

  for (int i = erizo::kArbitrarySeqNumber + 1; i < erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets; ++i) {
    EXPECT_TRUE(RtcpPacketContainsNackSeqNum(receiver_report, i)) << "i = " << i;
  }
}

TEST_F(RtcpNackGeneratorTest, shouldNotRetransmitNacksInmediately) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  nack_generator.handleRtpPacket(second_packet);

  receiver_report = generateRrWithNack();

  EXPECT_TRUE(RtcpPacketContainsNackSeqNum(receiver_report, erizo::kArbitrarySeqNumber + 1));

  receiver_report = generateRrWithNack();

  EXPECT_FALSE(RtcpPacketContainsNackSeqNum(receiver_report, erizo::kArbitrarySeqNumber + 1));
}

TEST_F(RtcpNackGeneratorTest, shouldRetransmitNacksAfterTime) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  nack_generator.handleRtpPacket(second_packet);

  receiver_report = generateRrWithNack();

  EXPECT_TRUE(RtcpPacketContainsNackSeqNum(receiver_report, erizo::kArbitrarySeqNumber + 1));

  advanceClockMs(100);
  receiver_report = generateRrWithNack();

  EXPECT_TRUE(RtcpPacketContainsNackSeqNum(receiver_report, erizo::kArbitrarySeqNumber + 1));
}

TEST_F(RtcpNackGeneratorTest, shouldNotRetransmitNacksMoreThanTwice) {
  uint16_t kArbitraryNumberOfPackets = 4;
  auto first_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto second_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + kArbitraryNumberOfPackets,
      VIDEO_PACKET);
  nack_generator.handleRtpPacket(first_packet);
  nack_generator.handleRtpPacket(second_packet);

  receiver_report = generateRrWithNack();

  EXPECT_TRUE(RtcpPacketContainsNackSeqNum(receiver_report, erizo::kArbitrarySeqNumber + 1));

  advanceClockMs(100);
  receiver_report = generateRrWithNack();
  EXPECT_TRUE(RtcpPacketContainsNackSeqNum(receiver_report, erizo::kArbitrarySeqNumber + 1));

  advanceClockMs(100);
  receiver_report = generateRrWithNack();
  EXPECT_FALSE(RtcpPacketContainsNackSeqNum(receiver_report, erizo::kArbitrarySeqNumber + 1));
}
