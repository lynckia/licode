#ifndef ERIZO_SRC_TEST_UTILS_TOOLS_H_
#define ERIZO_SRC_TEST_UTILS_TOOLS_H_

#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <Stats.h>

#include <queue>
#include <string>
#include <vector>

namespace erizo {

static constexpr uint16_t kVideoSsrc = 1;
static constexpr uint16_t kAudioSsrc = 2;
static constexpr uint16_t kArbitrarySeqNumber = 12;
static constexpr uint16_t kFirstSequenceNumber = 0;
static constexpr uint16_t kLastSequenceNumber = 65535;

class PacketTools {
 public:
  static std::shared_ptr<DataPacket> createDataPacket(uint16_t seq_number, packetType type) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setSeqNumber(seq_number);

    if (type == AUDIO_PACKET) {
      header->setSSRC(kAudioSsrc);
    } else {
      header->setSSRC(kVideoSsrc);
    }

    return std::make_shared<DataPacket>(0, reinterpret_cast<char*>(header), sizeof(erizo::RtpHeader), type);
  }

  static std::shared_ptr<DataPacket> createNack(uint ssrc, uint source_ssrc, uint16_t seq_number,
                                                packetType type, int additional_packets = 0) {
    erizo::RtcpHeader *nack = new erizo::RtcpHeader();
    nack->setPacketType(RTCP_RTP_Feedback_PT);
    nack->setBlockCount(1);
    nack->setSSRC(ssrc);
    nack->setSourceSSRC(source_ssrc);
    nack->setNackPid(seq_number);
    nack->setNackBlp(additional_packets);
    nack->setLength(3);
    char *buf = reinterpret_cast<char*>(nack);
    int len = (nack->getLength() + 1) * 4;
    return std::make_shared<DataPacket>(0, buf, len, type);
  }

  static std::shared_ptr<DataPacket> createReceiverReport(uint ssrc, uint source_ssrc,
                                                          uint16_t highest_seq_num, packetType type,
                                                          uint32_t last_sender_report = 0, uint32_t fraction_lost = 0) {
    erizo::RtcpHeader *receiver_report = new erizo::RtcpHeader();
    receiver_report->setPacketType(RTCP_Receiver_PT);
    receiver_report->setBlockCount(1);
    receiver_report->setSSRC(ssrc);
    receiver_report->setSourceSSRC(source_ssrc);
    receiver_report->setHighestSeqnum(highest_seq_num);
    receiver_report->setLastSr(last_sender_report);
    receiver_report->setFractionLost(fraction_lost);
    receiver_report->setLength(7);
    char *buf = reinterpret_cast<char*>(receiver_report);
    int len = (receiver_report->getLength() + 1) * 4;
    return std::make_shared<DataPacket>(0, buf, len, type);
  }

  static std::shared_ptr<DataPacket> createSenderReport(uint ssrc, packetType type,
      uint32_t packets_sent = 0, uint32_t octets_sent = 0, uint64_t ntp_timestamp = 0) {
    erizo::RtcpHeader *sender_report = new erizo::RtcpHeader();
    sender_report->setPacketType(RTCP_Sender_PT);
    sender_report->setBlockCount(1);
    sender_report->setSSRC(ssrc);
    sender_report->setLength(4);
    sender_report->setNtpTimestamp(ntp_timestamp);
    sender_report->setPacketsSent(packets_sent);
    sender_report->setOctetsSent(octets_sent);
    char *buf = reinterpret_cast<char*>(sender_report);
    int len = (sender_report->getLength() + 1) * 4;
    return std::make_shared<DataPacket>(0, buf, len, type);
  }

  static std::shared_ptr<DataPacket> createVP8Packet(uint16_t seq_number, bool is_keyframe, bool is_marker) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setPayloadType(96);
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

    auto packet = std::make_shared<DataPacket>(0, packet_buffer, 200, VIDEO_PACKET);
    packet->is_keyframe = is_keyframe;
    return packet;
  }

  static std::shared_ptr<DataPacket> createVP8Packet(uint16_t seq_number, uint32_t timestamp,
      bool is_keyframe, bool is_marker) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setPayloadType(96);
    header->setSeqNumber(seq_number);
    header->setSSRC(kVideoSsrc);
    header->setTimestamp(timestamp);
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

    auto packet = std::make_shared<DataPacket>(0, packet_buffer, 200, VIDEO_PACKET);
    packet->is_keyframe = is_keyframe;
    return packet;
  }

  static std::shared_ptr<DataPacket> createVP9Packet(uint16_t seq_number, bool is_keyframe, bool is_marker) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setPayloadType(98);
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

    *parsing_pointer = is_keyframe? 0x00: 0x40;

    auto packet = std::make_shared<DataPacket>(0, packet_buffer, 200, VIDEO_PACKET);
    packet->is_keyframe = is_keyframe;
    return packet;
  }

  static std::shared_ptr<erizo::DataPacket> createRembPacket(uint32_t bitrate) {
    erizo::RtcpHeader *remb_packet = new erizo::RtcpHeader();
    remb_packet->setPacketType(RTCP_PS_Feedback_PT);
    remb_packet->setBlockCount(RTCP_AFB);
    memcpy(&remb_packet->report.rembPacket.uniqueid, "REMB", 4);

    remb_packet->setSSRC(2);
    remb_packet->setSourceSSRC(1);
    remb_packet->setLength(5);
    remb_packet->setREMBBitRate(bitrate);
    remb_packet->setREMBNumSSRC(1);
    remb_packet->setREMBFeedSSRC(55554);
    int remb_length = (remb_packet->getLength() + 1) * 4;
    char *buf = reinterpret_cast<char*>(remb_packet);
    auto packet = std::make_shared<erizo::DataPacket>(0, buf, remb_length, erizo::OTHER_PACKET);
    delete remb_packet;
    return packet;
  }

  static std::shared_ptr<erizo::DataPacket> createPLI() {
    erizo::RtcpHeader *pli = new erizo::RtcpHeader();
    pli->setPacketType(RTCP_PS_Feedback_PT);
    pli->setBlockCount(1);
    pli->setSSRC(55554);
    pli->setSourceSSRC(1);
    pli->setLength(2);
    char *buf = reinterpret_cast<char*>(pli);
    int len = (pli->getLength() + 1) * 4;
    auto packet = std::make_shared<erizo::DataPacket>(0, buf, len, erizo::OTHER_PACKET);
    delete pli;
    return packet;
  }
};

using ::testing::_;

class BaseHandlerTest  {
 public:
  BaseHandlerTest() {}

  virtual void setHandler() = 0;
  virtual void afterPipelineSetup() {}

  virtual void internalSetUp() {
    simulated_clock = std::make_shared<erizo::SimulatedClock>();
    simulated_worker = std::make_shared<erizo::SimulatedWorker>(simulated_clock);
    simulated_worker->start();
    io_worker = std::make_shared<erizo::IOWorker>();
    io_worker->start();
    connection = std::make_shared<erizo::MockWebRtcConnection>(simulated_worker, io_worker, ice_config, rtp_maps);
    processor = std::make_shared<erizo::MockRtcpProcessor>();
    quality_manager = std::make_shared<erizo::MockQualityManager>();
    packet_buffer_service = std::make_shared<erizo::PacketBufferService>();
    stats = std::make_shared<erizo::Stats>();
    connection->setVideoSinkSSRC(erizo::kVideoSsrc);
    connection->setAudioSinkSSRC(erizo::kAudioSsrc);
    connection->setVideoSourceSSRC(erizo::kVideoSsrc);
    connection->setAudioSourceSSRC(erizo::kAudioSsrc);

    pipeline = Pipeline::create();
    reader = std::make_shared<erizo::Reader>();
    writer = std::make_shared<erizo::Writer>();

    EXPECT_CALL(*reader, notifyUpdate()).Times(testing::AtLeast(1));
    EXPECT_CALL(*writer, notifyUpdate()).Times(testing::AtLeast(1));

    EXPECT_CALL(*reader, read(_, _)).Times(testing::AtLeast(0));
    EXPECT_CALL(*writer, write(_, _)).Times(testing::AtLeast(0));

    std::shared_ptr<erizo::WebRtcConnection> connection_ptr = std::dynamic_pointer_cast<WebRtcConnection>(connection);
    pipeline->addService(connection_ptr);
    pipeline->addService(std::dynamic_pointer_cast<RtcpProcessor>(processor));
    pipeline->addService(std::dynamic_pointer_cast<QualityManager>(quality_manager));
    pipeline->addService(packet_buffer_service);
    pipeline->addService(stats);

    pipeline->addBack(writer);
    setHandler();
    pipeline->addBack(reader);
    pipeline->finalize();
    afterPipelineSetup();
  }

  virtual void executeTasksInNextMs(int time) {
    for (int step = 0; step < time + 1; step++) {
      simulated_worker->executePastScheduledTasks();
      simulated_clock->advanceTime(std::chrono::milliseconds(1));
    }
  }

  virtual void internalTearDown() {
  }

  IceConfig ice_config;
  std::vector<RtpMap> rtp_maps;
  std::shared_ptr<erizo::Stats> stats;
  std::shared_ptr<erizo::MockWebRtcConnection> connection;
  std::shared_ptr<erizo::MockRtcpProcessor> processor;
  std::shared_ptr<erizo::MockQualityManager> quality_manager;
  Pipeline::Ptr pipeline;
  std::shared_ptr<erizo::Reader> reader;
  std::shared_ptr<erizo::Writer> writer;
  std::shared_ptr<erizo::SimulatedClock> simulated_clock;
  std::shared_ptr<erizo::SimulatedWorker> simulated_worker;
  std::shared_ptr<erizo::IOWorker> io_worker;
  std::shared_ptr<erizo::PacketBufferService> packet_buffer_service;
  std::queue<std::shared_ptr<DataPacket>> packet_queue;
};

class HandlerTest : public ::testing::Test, public BaseHandlerTest {
 public:
  HandlerTest() {}

  virtual void setHandler() = 0;

 protected:
  virtual void SetUp() {
    internalSetUp();
  }

  virtual void TearDown() {
    internalTearDown();
  }
};

}  // namespace erizo

#endif  // ERIZO_SRC_TEST_UTILS_TOOLS_H_
