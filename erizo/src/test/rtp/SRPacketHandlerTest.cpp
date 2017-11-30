#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/SRPacketHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>
#include <MediaStream.h>

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
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::SRPacketHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;

class SRPacketHandlerTest : public erizo::HandlerTest {
 public:
  SRPacketHandlerTest() {}

 protected:
  void setHandler() {
    sr_handler = std::make_shared<SRPacketHandler>();
    pipeline->addBack(sr_handler);
  }

  std::shared_ptr<SRPacketHandler> sr_handler;
};

TEST_F(SRPacketHandlerTest, basicBehaviourShouldReadPackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(*reader.get(), read(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->read(packet);
}

TEST_F(SRPacketHandlerTest, basicBehaviourShouldWritePackets) {
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  pipeline->write(packet);
}

TEST_F(SRPacketHandlerTest, shouldRewritePacketsSent) {
  uint32_t kArbitraryPacketsSent = 500;
  uint32_t kArbitraryOctetsSent = 1000;
  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto sr_packet = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET, kArbitraryPacketsSent,
      kArbitraryOctetsSent);
  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::SenderReportHasPacketsSentValue(1))).Times(1);
  pipeline->write(packet);
  pipeline->write(sr_packet);
}

TEST_F(SRPacketHandlerTest, shouldRewriteOctetsSent) {
  uint32_t kArbitraryPacketsSent = 500;
  uint32_t kArbitraryOctetsSent = 1000;
  uint32_t kDataPacketSizeWithoutHeaders = 8;

  auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
  auto sr_packet = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET, kArbitraryPacketsSent,
      kArbitraryOctetsSent);
  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
  EXPECT_CALL(*writer.get(), write(_, _)).
    With(Args<1>(erizo::SenderReportHasOctetsSentValue(kDataPacketSizeWithoutHeaders))).Times(1);

  pipeline->write(packet);
  pipeline->write(sr_packet);
}
