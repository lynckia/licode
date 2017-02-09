#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread/Scheduler.h>
#include <rtp/RtpSlideShowHandler.h>
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
using erizo::RtpSlideShowHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;

class RtpSlideShowHandlerTest : public erizo::HandlerTest {
 public:
  RtpSlideShowHandlerTest() {
  }

 protected:
  void setHandler() {
    std::vector<RtpMap>& payloads = connection->getRemoteSdpInfo().getPayloadInfos();
    payloads.push_back({96, "VP8"});
    payloads.push_back({98, "VP9"});
    slideshow_handler = std::make_shared<RtpSlideShowHandler>();
    pipeline->addBack(slideshow_handler);
  }

  std::shared_ptr<RtpSlideShowHandler> slideshow_handler;
};

TEST_F(RtpSlideShowHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpSlideShowHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpSlideShowHandlerTest, shouldTransmitAllPacketsWhenInactive) {
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

TEST_F(RtpSlideShowHandlerTest, shouldTransmitJustKeyframesInVP9) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    packet_queue.push(erizo::PacketTools::createVP9Packet(seq_number, true, false));
    packet_queue.push(erizo::PacketTools::createVP9Packet(++seq_number, false, false));
    slideshow_handler->setSlideShowMode(true);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(1);
    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
}
TEST_F(RtpSlideShowHandlerTest, shouldTransmitFromBeginningOfKFrameToMarkerPacketsWhenActive) {
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

TEST_F(RtpSlideShowHandlerTest, shouldMantainSequenceNumberInSlideShow) {
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

TEST_F(RtpSlideShowHandlerTest, shouldAdjustSequenceNumberAfterSlideShow) {
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
