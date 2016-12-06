#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>

#include <rtp/RtpAudioMuteHandler.h>
#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <WebRtcConnection.h>

#include <vector>

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
using erizo::RtpMap;
using erizo::RtpAudioMuteHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using std::queue;

class MockWebRtcConnection: public WebRtcConnection {
 public:
  MockWebRtcConnection(std::shared_ptr<Worker> worker, const IceConfig &ice_config,
                       const std::vector<RtpMap> rtp_mappings) :
    WebRtcConnection(worker, "", ice_config, rtp_mappings, nullptr) {}

  virtual ~MockWebRtcConnection() {
  }
};

class Reader : public InboundHandler {
 public:
  MOCK_METHOD2(read, void(Context*, std::shared_ptr<dataPacket>));
};

class Writer : public OutboundHandler {
 public:
  MOCK_METHOD2(write, void(Context*, std::shared_ptr<dataPacket>));
};

class RtpAudioMuteHandlerTest : public ::testing::Test {
 public:
  RtpAudioMuteHandlerTest() : ice_config() {}

 protected:
  virtual void SetUp() {
    scheduler = std::make_shared<Scheduler>(1);
    worker = std::make_shared<Worker>(scheduler);
    connection = std::make_shared<MockWebRtcConnection>(worker, ice_config, rtp_maps);

    connection->setVideoSinkSSRC(kVideoSsrc);
    connection->setAudioSinkSSRC(kAudioSsrc);

    pipeline = Pipeline::create();
    audio_mute_handler = std::make_shared<RtpAudioMuteHandler>(connection.get());
    reader = std::make_shared<Reader>();
    writer = std::make_shared<Writer>();

    pipeline->addBack(writer);
    pipeline->addBack(audio_mute_handler);
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

    return std::make_shared<dataPacket>(0, reinterpret_cast<char*>(header), sizeof(erizo::RtpHeader), type);
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
    return std::make_shared<dataPacket>(0, buf, len, type);
  }

  std::shared_ptr<dataPacket> createReceiverReport(uint16_t highest_seq_num, packetType type) {
    erizo::RtcpHeader *receiver_report = new erizo::RtcpHeader();
    receiver_report->setPacketType(RTCP_Receiver_PT);
    receiver_report->setBlockCount(1);
    uint source_ssrc = type   == VIDEO_PACKET ? connection->getVideoSinkSSRC() : connection->getAudioSinkSSRC();
    uint ssrc = type == VIDEO_PACKET ? connection->getVideoSourceSSRC() : connection->getAudioSourceSSRC();
    receiver_report->setSSRC(ssrc);
    receiver_report->setSourceSSRC(source_ssrc);
    receiver_report->setHighestSeqnum(highest_seq_num);
    receiver_report->setLength(7);
    char *buf = reinterpret_cast<char*>(receiver_report);
    int len = (receiver_report->getLength() + 1) * 4;
    return std::make_shared<dataPacket>(0, buf, len, type);
  }

  IceConfig ice_config;
  std::vector<RtpMap> rtp_maps;
  std::shared_ptr<MockWebRtcConnection> connection;
  Pipeline::Ptr pipeline;
  std::shared_ptr<Reader> reader;
  std::shared_ptr<Writer> writer;
  std::shared_ptr<RtpAudioMuteHandler> audio_mute_handler;
  std::shared_ptr<Worker> worker;
  std::shared_ptr<Scheduler> scheduler;
  std::queue<std::shared_ptr<dataPacket>> packet_queue;
};

MATCHER_P(HasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)->data))->getSeqNumber() == seq_num;
}

MATCHER_P(NackHasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getNackPid() == seq_num;
}

MATCHER_P(ReceiverReportHasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getHighestSeqnum() == seq_num;
}

TEST_F(RtpAudioMuteHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpAudioMuteHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

