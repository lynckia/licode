#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/FecReceiverHandler.h>
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
using erizo::DataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpHeader;
using erizo::FecReceiverHandler;
using erizo::MockUlpfecReceiver;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;

class FecReceiverHandlerTest : public erizo::HandlerTest {
 public:
  FecReceiverHandlerTest() {}

 protected:
  void setHandler() {
    fec_receiver_handler = std::make_shared<FecReceiverHandler>();
    fec_receiver = new MockUlpfecReceiver();
    fec_receiver_handler->setFecReceiver(std::unique_ptr<webrtc::UlpfecReceiver>(fec_receiver));
    pipeline->addBack(fec_receiver_handler);
  }

  std::shared_ptr<FecReceiverHandler> fec_receiver_handler;
  MockUlpfecReceiver* fec_receiver;
};

TEST_F(FecReceiverHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->write(packet);
}

TEST_F(FecReceiverHandlerTest, shouldNotCallFecReceiverWhenReceivingREDpacketsAndDisabled) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    rtp_header->setPayloadType(RED_90000_PT);

    fec_receiver_handler->disable();

    EXPECT_CALL(*fec_receiver, AddReceivedRedPacket(_, _, _, _)).Times(0);
    EXPECT_CALL(*fec_receiver, ProcessReceivedFec()).Times(0);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->write(packet);
}

TEST_F(FecReceiverHandlerTest, shouldCallFecReceiverWhenReceivingREDpacketsAndEnabled) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    rtp_header->setPayloadType(RED_90000_PT);

    fec_receiver_handler->enable();

    EXPECT_CALL(*fec_receiver, AddReceivedRedPacket(_, _, _, _)).Times(1);
    EXPECT_CALL(*fec_receiver, ProcessReceivedFec()).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    pipeline->write(packet);
}

TEST_F(FecReceiverHandlerTest, shouldWritePacketsWhenCalledFromFecReceiver) {
    auto packet = erizo::PacketTools::createDataPacket(erizo::kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).
      With(Args<1>(erizo::RtpHasSequenceNumber(erizo::kArbitrarySeqNumber))).Times(1);

    fec_receiver_handler->OnRecoveredPacket(reinterpret_cast<const uint8_t*>(packet->data), packet->length);
}
