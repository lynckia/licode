#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/RtpRetransmissionHandler.h>
#include <rtp/RtpHeaders.h>
#include <stats/StatNode.h>
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
using erizo::DataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::RtpRetransmissionHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;

class RtpRetransmissionHandlerTest : public erizo::HandlerTest {
 public:
  RtpRetransmissionHandlerTest() {}

 protected:
  void setHandler() {
    clock = std::make_shared<erizo::SimulatedClock>();
    rtx_handler = std::make_shared<RtpRetransmissionHandler>(clock);
    pipeline->addBack(rtx_handler);
  }

  std::shared_ptr<RtpRetransmissionHandler> rtx_handler;
  std::shared_ptr<erizo::SimulatedClock> clock;
};

TEST_F(RtpRetransmissionHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpRetransmissionHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingNacksWithGoodSeqNum) {
    auto rtp_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    uint ssrc = media_stream->getVideoSourceSSRC();
    uint source_ssrc = media_stream->getVideoSinkSSRC();
    auto nack_packet = erizo::PacketTools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(2);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldNotRetransmitPackets_whenReceivingNacksWithBadSeqNum) {
    auto rtp_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    uint ssrc = media_stream->getVideoSourceSSRC();
    uint source_ssrc = media_stream->getVideoSinkSSRC();
    auto nack_packet = erizo::PacketTools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber + 1, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(0);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldNotRetransmitPackets_whenReceivingNacksFromDifferentType) {
    auto rtp_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    uint ssrc = media_stream->getAudioSourceSSRC();
    uint source_ssrc = media_stream->getAudioSinkSSRC();
    auto nack_packet = erizo::PacketTools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingWithSeqNumBeforeGeneralRollover) {
    uint ssrc = media_stream->getVideoSourceSSRC();
    uint source_ssrc = media_stream->getVideoSinkSSRC();
    auto nack_packet = erizo::PacketTools::createNack(ssrc, source_ssrc, erizo::kFirstSequenceNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kLastSequenceNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kFirstSequenceNumber))).Times(2);
    pipeline->write(erizo::PacketTools::createDataPacket(erizo::kLastSequenceNumber, VIDEO_PACKET));
    pipeline->write(erizo::PacketTools::createDataPacket(erizo::kFirstSequenceNumber, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingWithSeqNumBeforeBufferRollover) {
    uint ssrc = media_stream->getVideoSourceSSRC();
    uint source_ssrc = media_stream->getVideoSinkSSRC();
    auto nack_packet = erizo::PacketTools::createNack(ssrc, source_ssrc, kRetransmissionsBufferSize - 1, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
        With(Args<1>(erizo::RtpHasSequenceNumber(kRetransmissionsBufferSize))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
        With(Args<1>(erizo::RtpHasSequenceNumber(kRetransmissionsBufferSize - 1))).Times(2);
    pipeline->write(erizo::PacketTools::createDataPacket(kRetransmissionsBufferSize - 1, VIDEO_PACKET));
    pipeline->write(erizo::PacketTools::createDataPacket(kRetransmissionsBufferSize, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingNackWithMultipleSeqNums) {
    uint ssrc = media_stream->getVideoSourceSSRC();
    uint source_ssrc = media_stream->getVideoSinkSSRC();
    auto nack_packet = erizo::PacketTools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber, VIDEO_PACKET, 1);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(2);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(2);
    pipeline->write(erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET));
    pipeline->write(erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + 1, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}
