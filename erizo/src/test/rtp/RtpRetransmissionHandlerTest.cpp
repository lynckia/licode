#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Headers for RtpPacketQueue.h tests
#include <rtp/RtpRetransmissionHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

static constexpr uint16_t kVideoSsrc = 1;
static constexpr uint16_t kAudioSsrc = 2;
static constexpr uint16_t kArbitrarySeqNumber = 12;
static constexpr uint16_t kFirstSequenceNumber = 0;
static constexpr uint16_t kLastSequenceNumber = 65535;

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using erizo::dataPacket;
using erizo::packetType;
using erizo::AUDIO_PACKET;
using erizo::VIDEO_PACKET;
using erizo::IceConfig;
using erizo::RtpRetransmissionHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;

class MockWebRtcConnection: public WebRtcConnection {
 public:
  explicit MockWebRtcConnection(const IceConfig &ice_config) :
    WebRtcConnection("", true, true, ice_config, nullptr) {}

  virtual ~MockWebRtcConnection() {
  }

  MOCK_METHOD1(read, void(std::shared_ptr<dataPacket>));
  MOCK_METHOD1(write, void(std::shared_ptr<dataPacket>));
};

class Reader : public InboundHandler {
 public:
  MOCK_METHOD2(read, void(Context*, std::shared_ptr<dataPacket>));
};

class Writer : public OutboundHandler {
 public:
  MOCK_METHOD2(write, void(Context*, std::shared_ptr<dataPacket>));
};

class RtpRetransmissionHandlerTest : public ::testing::Test {
 public:
  RtpRetransmissionHandlerTest() : ice_config() {}

 protected:
  virtual void SetUp() {
    connection = std::make_shared<MockWebRtcConnection>(ice_config);

    connection->setVideoSinkSSRC(kVideoSsrc);
    connection->setAudioSinkSSRC(kAudioSsrc);

    pipeline = Pipeline::create();
    rtx_handler = std::make_shared<RtpRetransmissionHandler>(connection.get());
    reader = std::make_shared<Reader>();
    writer = std::make_shared<Writer>();

    pipeline->addBack(writer);
    pipeline->addBack(rtx_handler);
    pipeline->addBack(reader);
    pipeline->finalize();
  }

  virtual void TearDown() {
  }

  std::shared_ptr<dataPacket> createDataPacket(uint16_t seq_number, packetType type) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setSeqNumber(seq_number);

    if (type == AUDIO_PACKET) {
      header->setSSRC(kAudioSsrc);
    } else {
      header->setSSRC(kVideoSsrc);
    }

    return std::make_shared<dataPacket>(0, reinterpret_cast<char*>(header), sizeof(erizo::RtpHeader), type, 0);
  }

  std::shared_ptr<dataPacket> createNack(uint16_t seq_number, packetType type, int additional_packets = 0) {
    erizo::RtcpHeader *nack = new erizo::RtcpHeader();
    nack->setPacketType(RTCP_RTP_Feedback_PT);
    nack->setBlockCount(1);
    uint source_ssrc = type   == VIDEO_PACKET ? connection->getVideoSinkSSRC() : connection->getAudioSinkSSRC();
    uint ssrc = type == VIDEO_PACKET ? connection->getVideoSourceSSRC() : connection->getAudioSourceSSRC();
    nack->setSSRC(ssrc);
    nack->setSourceSSRC(source_ssrc);
    nack->setNackPid(seq_number);
    nack->setNackBlp(additional_packets);
    nack->setLength(3);
    char *buf = reinterpret_cast<char*>(nack);
    int len = (nack->getLength() + 1) * 4;
    return std::make_shared<dataPacket>(0, buf, len, type, 0);
  }

  IceConfig ice_config;
  std::shared_ptr<MockWebRtcConnection> connection;
  Pipeline::Ptr pipeline;
  std::shared_ptr<Reader> reader;
  std::shared_ptr<Writer> writer;
  std::shared_ptr<RtpRetransmissionHandler> rtx_handler;
};

MATCHER_P(HasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)->data))->getSeqNumber() == seq_num;
}

TEST_F(RtpRetransmissionHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpRetransmissionHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingNacksWithGoodSeqNum) {
    auto rtp_packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);
    auto nack_packet = createNack(kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(2);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldNotRetransmitPackets_whenReceivingNacksWithBadSeqNum) {
    auto rtp_packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);
    auto nack_packet = createNack(kArbitrarySeqNumber + 1, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 1))).Times(0);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldNotRetransmitPackets_whenReceivingNacksFromDifferentType) {
    auto rtp_packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);
    auto nack_packet = createNack(kArbitrarySeqNumber, AUDIO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    pipeline->write(rtp_packet);

    EXPECT_CALL(*reader.get(), read(_, _)).Times(1);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingWithSeqNumBeforeGeneralRollover) {
    auto nack_packet = createNack(kFirstSequenceNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kLastSequenceNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kFirstSequenceNumber))).Times(2);
    pipeline->write(createDataPacket(kLastSequenceNumber, VIDEO_PACKET));
    pipeline->write(createDataPacket(kFirstSequenceNumber, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingWithSeqNumBeforeBufferRollover) {
    auto nack_packet = createNack(kRetransmissionsBufferSize - 1, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kRetransmissionsBufferSize))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kRetransmissionsBufferSize - 1))).Times(2);
    pipeline->write(createDataPacket(kRetransmissionsBufferSize - 1, VIDEO_PACKET));
    pipeline->write(createDataPacket(kRetransmissionsBufferSize, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}

TEST_F(RtpRetransmissionHandlerTest, shouldRetransmitPackets_whenReceivingNackWithMultipleSeqNums) {
    auto nack_packet = createNack(kArbitrarySeqNumber, VIDEO_PACKET, 1);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(2);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 1))).Times(2);
    pipeline->write(createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET));
    pipeline->write(createDataPacket(kArbitrarySeqNumber + 1, VIDEO_PACKET));

    EXPECT_CALL(*reader.get(), read(_, _)).Times(0);
    pipeline->read(nack_packet);
}
