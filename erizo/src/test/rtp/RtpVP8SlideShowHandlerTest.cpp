#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>

#include <thread/Scheduler.h>
#include <rtp/RtpVP8SlideShowHandler.h>
#include <rtp/RtpVP8Parser.h>
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
using erizo::RtpVP8SlideShowHandler;
using erizo::WebRtcConnection;
using erizo::Pipeline;
using erizo::InboundHandler;
using erizo::OutboundHandler;
using erizo::Worker;
using erizo::RtpVP8Parser;
using erizo::RTPPayloadVP8;
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

class RtpVP8SlideShowHandlerTest : public ::testing::Test {
 public:
  RtpVP8SlideShowHandlerTest() : ice_config() {}

 protected:
  virtual void SetUp() {
    scheduler = std::make_shared<Scheduler>(1);
    worker = std::make_shared<Worker>(scheduler);
    connection = std::make_shared<MockWebRtcConnection>(worker, ice_config, rtp_maps);

    connection->setVideoSinkSSRC(kVideoSsrc);
    connection->setAudioSinkSSRC(kAudioSsrc);

    pipeline = Pipeline::create();
    slideshow_handler = std::make_shared<RtpVP8SlideShowHandler>(connection.get());
    reader = std::make_shared<Reader>();
    writer = std::make_shared<Writer>();

    pipeline->addBack(writer);
    pipeline->addBack(slideshow_handler);
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

  std::shared_ptr<dataPacket> createVP8Packet(uint16_t seq_number, bool is_keyframe, bool is_marker) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setSeqNumber(seq_number);
    header->setSSRC(kVideoSsrc);
    header->setMarker(is_marker);
    char packet_buffer[200];
    memset(packet_buffer, 0, 200);
    char* data_pointer;
    char* parsing_pointer;
    memcpy(packet_buffer, reinterpret_cast<char*>(header), header->getHeaderLength());
    data_pointer = packet_buffer + header->getHeaderLength();
    parsing_pointer = data_pointer;


    *parsing_pointer = 0x10;
    parsing_pointer++;
    *parsing_pointer = is_keyframe? 0x00: 0x01;

    return std::make_shared<dataPacket>(0, packet_buffer, 200, VIDEO_PACKET);
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
  std::shared_ptr<RtpVP8SlideShowHandler> slideshow_handler;
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

TEST_F(RtpVP8SlideShowHandlerTest, basicBehaviourShouldReadPackets) {
    auto packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*reader.get(), read(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    pipeline->read(packet);
}

TEST_F(RtpVP8SlideShowHandlerTest, basicBehaviourShouldWritePackets) {
    auto packet = createDataPacket(kArbitrarySeqNumber, VIDEO_PACKET);

    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    pipeline->write(packet);
}

TEST_F(RtpVP8SlideShowHandlerTest, shouldTransmitAllPacketsWhenInactive) {
    uint16_t seq_number = kArbitrarySeqNumber;
    packet_queue.push(createVP8Packet(seq_number, false, false));
    packet_queue.push(createVP8Packet(++seq_number, true, false));
    packet_queue.push(createVP8Packet(++seq_number, false, false));
    packet_queue.push(createVP8Packet(++seq_number, false, true));
    packet_queue.push(createVP8Packet(++seq_number, false, false));
    slideshow_handler->setSlideShowMode(false);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(5);

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
}
TEST_F(RtpVP8SlideShowHandlerTest, shouldTransmitFromBeginningOfKFrameToMarkerPacketsWhenActive) {
    uint16_t seq_number = kArbitrarySeqNumber;
    packet_queue.push(createVP8Packet(seq_number, false, false));
    packet_queue.push(createVP8Packet(++seq_number, true, false));
    packet_queue.push(createVP8Packet(++seq_number, false, false));
    packet_queue.push(createVP8Packet(++seq_number, false, true));
    packet_queue.push(createVP8Packet(++seq_number, false, false));
    slideshow_handler->setSlideShowMode(true);

    EXPECT_CALL(*writer.get(), write(_, _)).Times(3);
    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
}

TEST_F(RtpVP8SlideShowHandlerTest, shouldMantainSequenceNumberInSlideShow) {
    uint16_t seq_number = kArbitrarySeqNumber;
    packet_queue.push(createVP8Packet(seq_number, true, false));
    packet_queue.push(createVP8Packet(++seq_number, false, true));

    packet_queue.push(createVP8Packet(++seq_number, false, false));
    packet_queue.push(createVP8Packet(++seq_number, false, false));

    packet_queue.push(createVP8Packet(++seq_number, true, false));
    packet_queue.push(createVP8Packet(++seq_number, false, true));
    slideshow_handler->setSlideShowMode(true);


    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 1))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 2))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 3))).Times(1);

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
}

TEST_F(RtpVP8SlideShowHandlerTest, shouldAdjustSequenceNumberAfterSlideShow) {
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 1))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 2))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 3))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 4))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 5))).Times(1);
    EXPECT_CALL(*writer.get(), write(_, _)).With(Args<1>(HasSequenceNumber(kArbitrarySeqNumber + 6))).Times(1);

    uint16_t seq_number = kArbitrarySeqNumber;
    uint16_t packets_after_handler = 0;
    packet_queue.push(createVP8Packet(seq_number, true, false));
    packets_after_handler++;
    packet_queue.push(createVP8Packet(++seq_number, false, true));
    packets_after_handler++;

    packet_queue.push(createVP8Packet(++seq_number, false, false));
    packet_queue.push(createVP8Packet(++seq_number, false, false));

    packet_queue.push(createVP8Packet(++seq_number, true, false));
    packets_after_handler++;
    packet_queue.push(createVP8Packet(++seq_number, false, true));
    packets_after_handler++;

    slideshow_handler->setSlideShowMode(true);
    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }

    slideshow_handler->setSlideShowMode(false);
    packet_queue.push(createVP8Packet(++seq_number, false, true));
    packets_after_handler++;
    packet_queue.push(createVP8Packet(++seq_number, false, false));
    packets_after_handler++;
    packet_queue.push(createVP8Packet(++seq_number, false, false));

    uint16_t last_sent_seq_number = seq_number;

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(NackHasSequenceNumber(last_sent_seq_number))).
      Times(1);

    EXPECT_CALL(*reader.get(), read(_, _)).
      With(Args<1>(ReceiverReportHasSequenceNumber(last_sent_seq_number))).
      Times(1);

    while (!packet_queue.empty()) {
      pipeline->write(packet_queue.front());
      packet_queue.pop();
    }
    auto nack = createNack(kArbitrarySeqNumber + packets_after_handler, VIDEO_PACKET);
    pipeline->read(nack);
    auto receiver_report = createReceiverReport(kArbitrarySeqNumber + packets_after_handler, VIDEO_PACKET);
    pipeline->read(receiver_report);
}

