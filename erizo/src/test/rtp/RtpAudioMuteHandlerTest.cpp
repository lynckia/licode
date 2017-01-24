#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>

#include <rtp/RtpAudioMuteHandler.h>
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
using erizo::RtpAudioMuteHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;

class RtpAudioMuteHandlerTest : public ::testing::Test {
 public:
  RtpAudioMuteHandlerTest() : ice_config() {}

 protected:
  virtual void SetUp() {
    scheduler = std::make_shared<Scheduler>(1);
    worker = std::make_shared<Worker>(scheduler);
    connection = std::make_shared<erizo::MockWebRtcConnection>(worker, ice_config, rtp_maps);

    connection->setVideoSinkSSRC(erizo::kVideoSsrc);
    connection->setAudioSinkSSRC(erizo::kAudioSsrc);

    pipeline = Pipeline::create();
    audio_mute_handler = std::make_shared<RtpAudioMuteHandler>(connection.get());
    reader = std::make_shared<erizo::Reader>();
    writer = std::make_shared<erizo::Writer>();

    pipeline->addBack(writer);
    pipeline->addBack(audio_mute_handler);
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
  std::shared_ptr<RtpAudioMuteHandler> audio_mute_handler;
  std::shared_ptr<Worker> worker;
  std::shared_ptr<Scheduler> scheduler;
  std::queue<std::shared_ptr<dataPacket>> packet_queue;
};

TEST_F(RtpAudioMuteHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpAudioMuteHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpAudioMuteHandlerTest, shouldNotWriteAudioPacketsIfActive) {
    auto audio_packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);
    auto video_packet = erizo::Tools::createDataPacket(erizo::kArbitrarySeqNumber+1, VIDEO_PACKET);
    audio_mute_handler->muteAudio(true);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber+1))).Times(1);

    pipeline->write(audio_packet);
    pipeline->write(video_packet);
}

TEST_F(RtpAudioMuteHandlerTest, shouldAdjustSequenceNumbers) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber+1))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber+2))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(erizo::kArbitrarySeqNumber+3))).Times(1);

    packet_queue.push(erizo::Tools::createDataPacket(seq_number, AUDIO_PACKET));
    packet_queue.push(erizo::Tools::createDataPacket(++seq_number, AUDIO_PACKET));

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
    audio_mute_handler->muteAudio(true);
    packet_queue.push(erizo::Tools::createDataPacket(++seq_number, AUDIO_PACKET));
    packet_queue.push(erizo::Tools::createDataPacket(++seq_number, AUDIO_PACKET));

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
    audio_mute_handler->muteAudio(false);

    packet_queue.push(erizo::Tools::createDataPacket(++seq_number, AUDIO_PACKET));
    packet_queue.push(erizo::Tools::createDataPacket(++seq_number, AUDIO_PACKET));

    uint16_t last_sent_seq_number = seq_number;

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(NackHasSequenceNumber(last_sent_seq_number))).
      Times(1);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(ReceiverReportHasSequenceNumber(last_sent_seq_number))).
      Times(1);

    uint source_ssrc = connection->getAudioSinkSSRC();
    uint ssrc = connection->getAudioSourceSSRC();
    auto nack = erizo::Tools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber + 3, AUDIO_PACKET);
    pipeline->read(nack);
    auto receiver_report = erizo::Tools::createReceiverReport(ssrc, source_ssrc, erizo::kArbitrarySeqNumber + 3,
                                                              AUDIO_PACKET);
    pipeline->read(receiver_report);
}
