#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/SenderBandwidthEstimationHandler.h>
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
using ::testing::Eq;
using ::testing::Args;
using ::testing::Return;
using erizo::dataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::WebRtcConnection;
using erizo::SenderBandwidthEstimationHandler;
using erizo::SenderBandwidthEstimationListener;
using erizo::MockSenderBandwidthEstimationListener;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using std::queue;


class SenderBandwidthEstimationHandlerTest : public erizo::HandlerTest {
 public:
  SenderBandwidthEstimationHandlerTest() {}
 protected:
  void setHandler() {
    sender_estimator_handler = std::make_shared<SenderBandwidthEstimationHandler>();
    bandwidth_listener = std::make_shared<MockSenderBandwidthEstimationListener>();
    pipeline->addBack(sender_estimator_handler);
    sender_estimator_handler->setListener(bandwidth_listener.get());
  }

  std::shared_ptr<SenderBandwidthEstimationHandler> sender_estimator_handler;
  std::shared_ptr<MockSenderBandwidthEstimationListener>  bandwidth_listener;
  const uint32_t kArbitrarySsrc = 32;
  const uint64_t kArbitraryNtpTimestamp = 493248028403924389;
};

TEST_F(SenderBandwidthEstimationHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->read(packet);
}

TEST_F(SenderBandwidthEstimationHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->write(packet);
}

TEST_F(SenderBandwidthEstimationHandlerTest, shouldProvideEstimateOnCorrespondingReceiverReport) {
    const uint32_t kMiddle32BitsFromArbitraryNtpTimestamp = 1584918317;
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    auto sr_packet = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET,
        0, 0, kArbitraryNtpTimestamp);
    auto rr_packet = erizo::PacketTools::createReceiverReport(kArbitrarySsrc, erizo::kVideoSsrc,
        erizo::kArbitrarySeqNumber, VIDEO_PACKET, kMiddle32BitsFromArbitraryNtpTimestamp);

    EXPECT_CALL(*bandwidth_listener.get(), onBandwidthEstimate(_, _, _)).Times(1);
    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).Times(2);

    pipeline->write(packet);
    pipeline->write(sr_packet);
    pipeline->read(rr_packet);
}

TEST_F(SenderBandwidthEstimationHandlerTest, shouldNotProvideEstimateOnNonCorrespondingReceiverReport) {
    const uint32_t kArbitraryMiddle32BitsNTP = 49;
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    auto sr_packet = erizo::PacketTools::createSenderReport(erizo::kVideoSsrc, VIDEO_PACKET,
        0, 0, kArbitraryNtpTimestamp);
    auto rr_packet = erizo::PacketTools::createReceiverReport(kArbitrarySsrc, erizo::kVideoSsrc,
        erizo::kArbitrarySeqNumber, VIDEO_PACKET, kArbitraryMiddle32BitsNTP);

    EXPECT_CALL(*bandwidth_listener.get(), onBandwidthEstimate(_, _, _)).Times(0);
    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).Times(2);

    pipeline->write(packet);
    pipeline->write(sr_packet);
    pipeline->read(rr_packet);
}

TEST_F(SenderBandwidthEstimationHandlerTest, shouldProvideEstimateOnREMB) {
    int kArbitraryBitrate = 5000000;
    auto remb_packet = erizo::PacketTools::createRembPacket(kArbitraryBitrate);
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    EXPECT_CALL(*writer.get(), write(_, _)).Times(1);
    EXPECT_CALL(*bandwidth_listener.get(), onBandwidthEstimate(_, _, _)).Times(1);
    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);

    pipeline->write(packet);
    pipeline->read(remb_packet);
}

