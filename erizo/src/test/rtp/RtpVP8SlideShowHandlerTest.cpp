#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/RtpVP8SlideShowHandler.h>
#include <rtp/RtpVP8Parser.h>
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
using ::testing::Args;
using ::testing::Return;
using erizo::dataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::RtpVP8SlideShowHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using erizo::RtpVP8Parser;
using erizo::RTPPayloadVP8;
using std::queue;

class RtpVP8SlideShowHandlerTest : public ::testing::Test {
 public:
  RtpVP8SlideShowHandlerTest() : ice_config() {}

 protected:
  virtual void SetUp() {
    scheduler = std::make_shared<Scheduler>(1);
    worker = std::make_shared<Worker>(scheduler);
    connection = std::make_shared<erizo::MockWebRtcConnection>(worker, ice_config, rtp_maps);

    connection->setVideoSinkSSRC(erizo::kVideoSsrc);
    connection->setAudioSinkSSRC(erizo::kAudioSsrc);

    pipeline = Pipeline::create();
    slideshow_handler = std::make_shared<RtpVP8SlideShowHandler>(connection.get());
    reader = std::make_shared<erizo::Reader>();
    writer = std::make_shared<erizo::Writer>();

    pipeline->addBack(writer);
    pipeline->addBack(slideshow_handler);
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
  std::shared_ptr<RtpVP8SlideShowHandler> slideshow_handler;
  std::shared_ptr<Worker> worker;
  std::shared_ptr<Scheduler> scheduler;
  std::queue<std::shared_ptr<dataPacket>> packet_queue;
};

TEST_F(RtpVP8SlideShowHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpVP8SlideShowHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpVP8SlideShowHandlerTest, shouldTransmitAllPacketsWhenInactive) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, true, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, true));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));
    slideshow_handler->setSlideShowMode(false);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(5);

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
}
TEST_F(RtpVP8SlideShowHandlerTest, shouldTransmitFromBeginningOfKFrameToMarkerPacketsWhenActive) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, true, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, true));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));
    slideshow_handler->setSlideShowMode(true);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(3);
    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
}

TEST_F(RtpVP8SlideShowHandlerTest, shouldMantainSequenceNumberInSlideShow) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number, true, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, true));

    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));

    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, true, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, true));
    slideshow_handler->setSlideShowMode(true);


    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 2))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 3))).Times(1);

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
}

TEST_F(RtpVP8SlideShowHandlerTest, shouldAdjustSequenceNumberAfterSlideShow) {
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 2))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 3))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 4))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 5))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 6))).Times(1);

    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    uint16_t packets_after_handler = 0;
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number, true, false));
    packets_after_handler++;
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, true));
    packets_after_handler++;

    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));

    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, true, false));
    packets_after_handler++;
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, true));
    packets_after_handler++;

    slideshow_handler->setSlideShowMode(true);
    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }

    slideshow_handler->setSlideShowMode(false);
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, true));
    packets_after_handler++;
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));
    packets_after_handler++;
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, false, false));

    uint16_t last_sent_seq_number = seq_number;

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::NackHasSequenceNumber(last_sent_seq_number))).
      Times(1);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::ReceiverReportHasSequenceNumber(last_sent_seq_number))).
      Times(1);

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
    uint ssrc = connection->getVideoSourceSSRC();
    uint source_ssrc = connection->getVideoSinkSSRC();
    auto nack = erizo::PacketTools::createNack(ssrc, source_ssrc,
                                erizo::kArbitrarySeqNumber + packets_after_handler, VIDEO_PACKET);
    pipeline->read(nack);
    auto receiver_report = erizo::PacketTools::createReceiverReport(ssrc, source_ssrc,
                                erizo::kArbitrarySeqNumber + packets_after_handler, VIDEO_PACKET);
    pipeline->read(receiver_report);
}
