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


class RtpAudioMuteHandlerTest : public erizo::HandlerTest {
 public:
  RtpAudioMuteHandlerTest() {}

 protected:
  void setHandler() {
    audio_mute_handler = std::make_shared<RtpAudioMuteHandler>();
    pipeline->addBack(audio_mute_handler);
  }

  std::shared_ptr<RtpAudioMuteHandler> audio_mute_handler;
};

TEST_F(RtpAudioMuteHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpAudioMuteHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpAudioMuteHandlerTest, shouldNotWriteAudioPacketsIfActive) {
    auto audio_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);
    auto video_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber+1, VIDEO_PACKET);
    audio_mute_handler->muteAudio(true);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber+1))).Times(1);

    pipeline->write(audio_packet);
    pipeline->write(video_packet);
}

TEST_F(RtpAudioMuteHandlerTest, shouldAdjustSequenceNumbers) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber+1))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber+2))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber+3))).Times(1);

    packet_queue.push(erizo::PacketTools::createDataPacket(seq_number, AUDIO_PACKET));
    packet_queue.push(erizo::PacketTools::createDataPacket(++seq_number, AUDIO_PACKET));

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
    audio_mute_handler->muteAudio(true);
    packet_queue.push(erizo::PacketTools::createDataPacket(++seq_number, AUDIO_PACKET));
    packet_queue.push(erizo::PacketTools::createDataPacket(++seq_number, AUDIO_PACKET));

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
    audio_mute_handler->muteAudio(false);

    packet_queue.push(erizo::PacketTools::createDataPacket(++seq_number, AUDIO_PACKET));
    packet_queue.push(erizo::PacketTools::createDataPacket(++seq_number, AUDIO_PACKET));

    uint16_t last_sent_seq_number = seq_number;

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::NackHasSequenceNumber(last_sent_seq_number))).
      Times(1);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::ReceiverReportHasSequenceNumber(last_sent_seq_number))).
      Times(1);

    uint source_ssrc = connection->getAudioSinkSSRC();
    uint ssrc = connection->getAudioSourceSSRC();
    auto nack = erizo::PacketTools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber + 3, AUDIO_PACKET);
    pipeline->read(nack);
    auto receiver_report = erizo::PacketTools::createReceiverReport(ssrc, source_ssrc, erizo::kArbitrarySeqNumber + 3,
                                                              AUDIO_PACKET);
    pipeline->read(receiver_report);
}
