#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/RtpRetransmissionHandler.h>
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
using erizo::dataPacket;
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

class RtpRetransmissionHandlerTest : public ::testing::Test {
 public:
  RtpRetransmissionHandlerTest() : ice_config() {}

 protected:
  virtual void SetUp() {
    scheduler = std::make_shared<Scheduler>(1);
    worker = std::make_shared<Worker>(scheduler);
    connection = std::make_shared<erizo::MockWebRtcConnection>(worker, ice_config, rtp_maps);

    connection->setVideoSinkSSRC(erizo::kVideoSsrc);
    connection->setAudioSinkSSRC(erizo::kAudioSsrc);

    pipeline = Pipeline::create();
    rtx_handler = std::make_shared<RtpRetransmissionHandler>(connection.get());
    reader = std::make_shared<erizo::Reader>();
    writer = std::make_shared<erizo::Writer>();

    pipeline->addBack(writer);
    pipeline->addBack(rtx_handler);
    pipeline->addBack(reader);
    pipeline->finalize();
  }

  virtual void TearDown() {
  }

  IceConfig ice_config;
  std::vector<RtpMap> rtp_maps;
  std::shared_ptr<erizo::MockWebRtcConnection> connection;
  Pipeline::Ptr pipeline;
  std::shared_ptr<erizo::Reader> reader;
  std::shared_ptr<erizo::Writer> writer;
  std::shared_ptr<RtpRetransmissionHandler> rtx_handler;
  std::shared_ptr<Worker> worker;
  std::shared_ptr<Scheduler> scheduler;
};

TEST_F(RtpRetransmissionHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpRetransmissionHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingNacksWithGoodSeqNum) {
    auto rtp_packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    uint ssrc = connection->getVideoSourceSSRC();
    uint source_ssrc = connection->getVideoSinkSSRC();
    auto nack_packet = erizo::Tools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(2);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldNotRetransmitPackets_whenReceivingNacksWithBadSeqNum) {
    auto rtp_packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    uint ssrc = connection->getVideoSourceSSRC();
    uint source_ssrc = connection->getVideoSinkSSRC();
    auto nack_packet = erizo::Tools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber + 1, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(0);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldNotRetransmitPackets_whenReceivingNacksFromDifferentType) {
    auto rtp_packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    uint ssrc = connection->getAudioSourceSSRC();
    uint source_ssrc = connection->getAudioSinkSSRC();
    auto nack_packet = erizo::Tools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingWithSeqNumBeforeGeneralRollover) {
    uint ssrc = connection->getVideoSourceSSRC();
    uint source_ssrc = connection->getVideoSinkSSRC();
    auto nack_packet = erizo::Tools::createNack(ssrc, source_ssrc, erizo::kFirstSequenceNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kLastSequenceNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kFirstSequenceNumber))).Times(2);
    pipeline->write(erizo::Tools::createDataPacket(erizo::kLastSequenceNumber, VIDEO_PACKET));
    pipeline->write(erizo::Tools::createDataPacket(erizo::kFirstSequenceNumber, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingWithSeqNumBeforeBufferRollover) {
    uint ssrc = connection->getVideoSourceSSRC();
    uint source_ssrc = connection->getVideoSinkSSRC();
    auto nack_packet = erizo::Tools::createNack(ssrc, source_ssrc, kRetransmissionsBufferSize - 1, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
        With(Args<1>(HasSequenceNumber(kRetransmissionsBufferSize))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
        With(Args<1>(HasSequenceNumber(kRetransmissionsBufferSize - 1))).Times(2);
    pipeline->write(erizo::Tools::createDataPacket(kRetransmissionsBufferSize - 1, VIDEO_PACKET));
    pipeline->write(erizo::Tools::createDataPacket(kRetransmissionsBufferSize, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingNackWithMultipleSeqNums) {
    uint ssrc = connection->getVideoSourceSSRC();
    uint source_ssrc = connection->getVideoSinkSSRC();
    auto nack_packet = erizo::Tools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber, VIDEO_PACKET, 1);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(2);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(2);
    pipeline->write(erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET));
    pipeline->write(erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber + 1, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}
