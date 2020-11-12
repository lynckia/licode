#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>

#include <rtp/RtpTrackMuteHandler.h>
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
using erizo::DataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpMap;
using erizo::RtpTrackMuteHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;


class RtpTrackMuteHandlerTest : public erizo::HandlerTest {
 public:
  RtpTrackMuteHandlerTest() {}

 protected:
  void setHandler() {
    track_mute_handler = std::make_shared<RtpTrackMuteHandler>(simulated_clock);
    pipeline->addBack(track_mute_handler);
  }

  void advanceClock(erizo::duration time) {
    simulated_clock->advanceTime(time);
  }

  std::shared_ptr<RtpTrackMuteHandler> track_mute_handler;
};

TEST_F(RtpTrackMuteHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpTrackMuteHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpTrackMuteHandlerTest, shouldNotWriteAudioPacketsIfActive) {
    auto audio_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);
    auto video_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber+1, VIDEO_PACKET);
    track_mute_handler->muteAudio(true);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(0);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(1);

    pipeline->write(audio_packet);
    pipeline->write(video_packet);
}

TEST_F(RtpTrackMuteHandlerTest, shouldNotWriteVideoPacketsIfActive) {
    auto audio_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);
    auto video_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber+1, VIDEO_PACKET);
    track_mute_handler->muteVideo(true);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(0);
    pipeline->write(audio_packet);
    pipeline->write(video_packet);
}

TEST_F(RtpTrackMuteHandlerTest, shouldTransformKeyframes) {
    auto keyframe = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, true, true);
    auto video_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber + 1, VIDEO_PACKET);
    track_mute_handler->muteVideo(true);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(0);
    pipeline->write(keyframe);
    pipeline->write(video_packet);
}

TEST_F(RtpTrackMuteHandlerTest, shouldTransformNormalPacketsEverykMuteVideoKeyframeTimeout) {
    auto video_packet = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber, false, false);
    auto video_packet2 = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 1, false, false);
    auto video_packet3 = erizo::PacketTools::createVP8Packet(erizo::kArbitrarySeqNumber + 2, false, false);
    track_mute_handler->muteVideo(true);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 2))).Times(0);
    pipeline->write(video_packet);
    pipeline->write(video_packet2);
    advanceClock(kMuteVideoKeyframeTimeout + std::chrono::milliseconds(1));
    pipeline->write(video_packet3);
}

TEST_F(RtpTrackMuteHandlerTest, shouldNotWriteAnyPacketsIfAllIsActive) {
    auto audio_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, AUDIO_PACKET);
    auto video_packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber+1, VIDEO_PACKET);
    track_mute_handler->muteVideo(true);
    track_mute_handler->muteAudio(true);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(0);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(0);

    pipeline->write(audio_packet);
    pipeline->write(video_packet);
}

TEST_F(RtpTrackMuteHandlerTest, shouldSendPLIsWhenVideoIsActivated) {
    track_mute_handler->muteVideo(true);

    EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(erizo::IsPLI())).Times(1);

    track_mute_handler->muteVideo(false);
}

TEST_F(RtpTrackMuteHandlerTest, shouldAdjustSequenceNumbers) {
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
    track_mute_handler->muteAudio(true);
    packet_queue.push(erizo::PacketTools::createDataPacket(++seq_number, AUDIO_PACKET));
    packet_queue.push(erizo::PacketTools::createDataPacket(++seq_number, AUDIO_PACKET));

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
    track_mute_handler->muteAudio(false);

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

    uint source_ssrc = media_stream->getAudioSinkSSRC();
    uint ssrc = media_stream->getAudioSourceSSRC();
    auto nack = erizo::PacketTools::createNack(ssrc, source_ssrc, erizo::kArbitrarySeqNumber + 3, AUDIO_PACKET);
    pipeline->read(nack);
    auto receiver_report = erizo::PacketTools::createReceiverReport(ssrc, source_ssrc, erizo::kArbitrarySeqNumber + 3,
                                                              AUDIO_PACKET);
    pipeline->read(receiver_report);
}
