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
using erizo::DataPacket;
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

const uint32_t kArbitraryTimestamp = 1000;

class RtpSlideShowHandlerTest : public erizo::HandlerTest {
 public:
  RtpSlideShowHandlerTest() {
  }

 protected:
  void setHandler() {
    std::vector<RtpMap>& payloads = media_stream->getRemoteSdpInfo()->getPayloadInfos();
    payloads.push_back({96, "VP8"});
    payloads.push_back({98, "VP9"});
    clock = std::make_shared<erizo::SimulatedClock>();
    slideshow_handler = std::make_shared<RtpSlideShowHandler>(clock);
    pipeline->addBack(slideshow_handler);
  }

  std::shared_ptr<erizo::SimulatedClock> clock;
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
TEST_F(RtpSlideShowHandlerTest, shouldTransmitFromBeginningOfKFrameToTimestampChange) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number, kArbitraryTimestamp - 1, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp, true, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp, false, true));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp + 1, false, false));
    slideshow_handler->setSlideShowMode(true);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(3);
    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
}

TEST_F(RtpSlideShowHandlerTest, shouldIgnoreKeyframeIfIsKeyframeIsNotFirstPacket) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number, kArbitraryTimestamp - 1, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number + 2, kArbitraryTimestamp, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number + 1, kArbitraryTimestamp, true, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp, false, true));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp + 1, false, false));
    slideshow_handler->setSlideShowMode(true);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(0);
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

TEST_F(RtpSlideShowHandlerTest, shouldResendKeyframesAfterKeyframeTimeout) {
    uint16_t seq_number = erizo::kArbitrarySeqNumber;
    slideshow_handler->setSlideShowMode(true);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 1))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 2))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber + 3))).Times(1);

    pipeline->write(erizo::PacketTools::createVP8Packet(seq_number, kArbitraryTimestamp, true, false));
    pipeline->write(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp, false, true));

    pipeline->write(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp + 1, false, false));

    clock->advanceTime(std::chrono::seconds(10));
    pipeline->write(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp + 2, false, false));
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
    packet_queue.push(erizo::PacketTools::createVP8Packet(seq_number, kArbitraryTimestamp, true, false));
    packets_after_handler++;
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp, false, true));
    packets_after_handler++;

    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp + 1, false, false));
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp + 2, false, false));

    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp + 3, true, false));
    packets_after_handler++;
    packet_queue.push(erizo::PacketTools::createVP8Packet(++seq_number, kArbitraryTimestamp + 3, false, true));
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
    uint ssrc = media_stream->getVideoSourceSSRC();
    uint source_ssrc = media_stream->getVideoSinkSSRC();
    auto nack = erizo::PacketTools::createNack(ssrc, source_ssrc,
                                erizo::kArbitrarySeqNumber + packets_after_handler, VIDEO_PACKET);
    pipeline->read(nack);
    auto receiver_report = erizo::PacketTools::createReceiverReport(ssrc, source_ssrc,
                                erizo::kArbitrarySeqNumber + packets_after_handler, VIDEO_PACKET);
    pipeline->read(receiver_report);
}
