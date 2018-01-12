#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <media/Depacketizer.h>

#include "../utils/Mocks.h"
#include "../utils/Tools.h"
#include "../utils/Matchers.h"

namespace {
  constexpr uint16_t seq = 44444;
  constexpr uint32_t timestamp = 0;
}

class DepacketizerVp8Test : public erizo::HandlerTest {
 public:
  DepacketizerVp8Test() {}

 protected:
  void setHandler() {
    depacketizer = std::make_shared<erizo::Vp8Depacketizer>();
  }

  int Vp8PacketDataSize() {
    const auto pkt = erizo::PacketTools::createVP8Packet(seq, false, false);
    const erizo::RtpHeader* head = reinterpret_cast<const erizo::RtpHeader*>(pkt->data);
    return pkt->length - head->getHeaderLength() - 1;  // subtract 1 to skip vp8 header
  }

  std::shared_ptr<erizo::Depacketizer> depacketizer;
};

class DepacketizerH264Test : public erizo::HandlerTest {
 public:
  DepacketizerH264Test() {}

 protected:
  void setHandler() {
    depacketizer = std::make_shared<erizo::H264Depacketizer>();
  }

  int H264SingleNalDataSize() {
    const auto pkt = erizo::PacketTools::createH264SingleNalPacket(seq, 0, false);
    const erizo::RtpHeader* head = reinterpret_cast<const erizo::RtpHeader*>(pkt->data);
    return pkt->length - head->getHeaderLength() + sizeof(erizo::RTPPayloadH264::start_sequence);
  }

  int H264FragmentedFrameDataSize() {
    const auto pkt = erizo::PacketTools::createH264FragmentedPacket(seq, 0, false, false, false);
    const erizo::RtpHeader* head = reinterpret_cast<const erizo::RtpHeader*>(pkt->data);
    return pkt->length - head->getHeaderLength() - 2;  // -2 = fa header
  }

  std::shared_ptr<erizo::Depacketizer> depacketizer;
};

TEST_F(DepacketizerVp8Test, isReallyEmpty) {
  EXPECT_EQ(0, depacketizer->isKeyframe());
  EXPECT_EQ(0, depacketizer->frameSize());
}

TEST_F(DepacketizerVp8Test, shouldDepacketSimplePacketFrame) {
  const auto pkt = erizo::PacketTools::createVP8Packet(seq, false, true);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  EXPECT_EQ(0, depacketizer->isKeyframe());
  EXPECT_EQ(Vp8PacketDataSize(), depacketizer->frameSize());
}

TEST_F(DepacketizerVp8Test, shouldDepacketSimplePacketKeyframe) {
  const auto pkt = erizo::PacketTools::createVP8Packet(seq, true, true);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  EXPECT_EQ(1, depacketizer->isKeyframe());
  EXPECT_EQ(Vp8PacketDataSize(), depacketizer->frameSize());
}

TEST_F(DepacketizerVp8Test, shouldDepacketMultiPacketFrame) {
  auto pkt = erizo::PacketTools::createVP8Packet(seq, false, false);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(0, depacketizer->processPacket());
  EXPECT_EQ(0, depacketizer->isKeyframe());

  pkt = erizo::PacketTools::createVP8Packet(seq + 1, false, false);
  // unset the bit indicating the start of partition
  unsigned char* pkt_ptr = reinterpret_cast<unsigned char*>(pkt->data);
  erizo::RtpHeader* head = reinterpret_cast<erizo::RtpHeader*>(pkt_ptr);
  pkt_ptr += head->getHeaderLength();
  *pkt_ptr = erizo::change_bit(*pkt_ptr, 4, false);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(0, depacketizer->processPacket());
  EXPECT_EQ(0, depacketizer->isKeyframe());

  pkt = erizo::PacketTools::createVP8Packet(seq + 2, false, true);
  // unset the bit indicating the start of partition
  pkt_ptr = reinterpret_cast<unsigned char*>(pkt->data);
  head = reinterpret_cast<erizo::RtpHeader*>(pkt_ptr);
  pkt_ptr += head->getHeaderLength();
  *pkt_ptr = erizo::change_bit(*pkt_ptr, 4, false);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  EXPECT_EQ(0, depacketizer->isKeyframe());
  EXPECT_EQ(Vp8PacketDataSize() * 3, depacketizer->frameSize());
}

TEST_F(DepacketizerVp8Test, shouldReset) {
  const auto pkt = erizo::PacketTools::createVP8Packet(seq, true, true);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  depacketizer->reset();
  EXPECT_EQ(0, depacketizer->isKeyframe());
  EXPECT_EQ(0, depacketizer->frameSize());
}

TEST_F(DepacketizerH264Test, isReallyEmpty) {
  EXPECT_EQ(0, depacketizer->isKeyframe());
  EXPECT_EQ(0, depacketizer->frameSize());
}

TEST_F(DepacketizerH264Test, shouldDepacketSingleNalFrame) {
  const auto pkt = erizo::PacketTools::createH264SingleNalPacket(seq, timestamp, false);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  EXPECT_EQ(0, depacketizer->isKeyframe());
  EXPECT_EQ(H264SingleNalDataSize(), depacketizer->frameSize());
}

TEST_F(DepacketizerH264Test, shouldDepacketSingleNalKeyframe) {
  const auto pkt = erizo::PacketTools::createH264SingleNalPacket(seq, timestamp, true);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  EXPECT_EQ(1, depacketizer->isKeyframe());
  EXPECT_EQ(H264SingleNalDataSize(), depacketizer->frameSize());
}

TEST_F(DepacketizerH264Test, shouldDepacketAggregatedFrame) {
  const unsigned char nal_1_len = 100;
  const unsigned char nal_2_len = 187;
  const auto pkt = erizo::PacketTools::createH264AggregatedPacket(seq, timestamp, nal_1_len, nal_2_len);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  EXPECT_EQ(0, depacketizer->isKeyframe());
  EXPECT_EQ(nal_1_len + nal_2_len + sizeof(erizo::RTPPayloadH264::start_sequence) * 2,
      static_cast<unsigned>(depacketizer->frameSize()));
}

TEST_F(DepacketizerH264Test, shouldDepacketFragmentedFrame) {
  auto pkt = erizo::PacketTools::createH264FragmentedPacket(seq, timestamp, true, false, false);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(0, depacketizer->processPacket());
  EXPECT_EQ(0, depacketizer->isKeyframe());

  pkt = erizo::PacketTools::createH264FragmentedPacket(seq, timestamp, false, true, false);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  EXPECT_EQ(0, depacketizer->isKeyframe());

  EXPECT_EQ(H264FragmentedFrameDataSize() * 2 +                // 2 packets
          sizeof(erizo::RTPPayloadH264::start_sequence) +      // 1 start sequence
          1,                                                   // 1 nal header
      static_cast<unsigned>(depacketizer->frameSize()));
}

TEST_F(DepacketizerH264Test, shouldReset) {
  const auto pkt = erizo::PacketTools::createH264SingleNalPacket(seq, timestamp, true);
  depacketizer->fetchPacket(reinterpret_cast<unsigned char*>(pkt->data), pkt->length);
  EXPECT_EQ(1, depacketizer->processPacket());
  depacketizer->reset();
  EXPECT_EQ(0, depacketizer->isKeyframe());
  EXPECT_EQ(0, depacketizer->frameSize());
}
