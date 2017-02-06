#ifndef ERIZO_SRC_TEST_UTILS_TOOLS_H_
#define ERIZO_SRC_TEST_UTILS_TOOLS_H_

#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>

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
  static std::shared_ptr<dataPacket> createDataPacket(uint16_t seq_number, packetType type) {
    erizo::RtpHeader *header = new erizo::RtpHeader();
    header->setSeqNumber(seq_number);

    if (type == AUDIO_PACKET) {
      header->setSSRC(kAudioSsrc);
    } else {
      header->setSSRC(kVideoSsrc);
    }

    return std::make_shared<dataPacket>(0, reinterpret_cast<char*>(header), sizeof(erizo::RtpHeader), type);
  }

  static std::shared_ptr<dataPacket> createNack(uint ssrc, uint source_ssrc, uint16_t seq_number,
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
    return std::make_shared<dataPacket>(0, buf, len, type);
  }

  static std::shared_ptr<dataPacket> createReceiverReport(uint ssrc, uint source_ssrc,
                                                          uint16_t highest_seq_num, packetType type) {
    erizo::RtcpHeader *receiver_report = new erizo::RtcpHeader();
    receiver_report->setPacketType(RTCP_Receiver_PT);
    receiver_report->setBlockCount(1);
    receiver_report->setSSRC(ssrc);
    receiver_report->setSourceSSRC(source_ssrc);
    receiver_report->setHighestSeqnum(highest_seq_num);
    receiver_report->setLength(7);
    char *buf = reinterpret_cast<char*>(receiver_report);
    int len = (receiver_report->getLength() + 1) * 4;
    return std::make_shared<dataPacket>(0, buf, len, type);
  }

  static std::shared_ptr<dataPacket> createSenderReport(uint ssrc, packetType type) {
    erizo::RtcpHeader *sender_report = new erizo::RtcpHeader();
    sender_report->setPacketType(RTCP_Sender_PT);
    sender_report->setBlockCount(1);
    sender_report->setSSRC(ssrc);
    sender_report->setLength(4);
    char *buf = reinterpret_cast<char*>(sender_report);
    int len = (sender_report->getLength() + 1) * 4;
    return std::make_shared<dataPacket>(0, buf, len, type);
  }

  static std::shared_ptr<dataPacket> createVP8Packet(uint16_t seq_number, bool is_keyframe, bool is_marker) {
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

    return std::make_shared<dataPacket>(0, packet_buffer, 200, VIDEO_PACKET);
  }

  static std::shared_ptr<dataPacket> createVP9Packet(uint16_t seq_number, bool is_keyframe, bool is_marker) {
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

    return std::make_shared<dataPacket>(0, packet_buffer, 200, VIDEO_PACKET);
  }

  static std::shared_ptr<erizo::dataPacket> createRembPacket(uint32_t bitrate) {
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
    auto packet = std::make_shared<erizo::dataPacket>(0, buf, remb_length, erizo::OTHER_PACKET);
    delete remb_packet;
    return packet;
  }

  static std::shared_ptr<erizo::dataPacket> createPLI() {
    erizo::RtcpHeader *pli = new erizo::RtcpHeader();
    pli->setPacketType(RTCP_PS_Feedback_PT);
    pli->setBlockCount(1);
    pli->setSSRC(55554);
    pli->setSourceSSRC(1);
    pli->setLength(2);
    char *buf = reinterpret_cast<char*>(pli);
    int len = (pli->getLength() + 1) * 4;
    auto packet = std::make_shared<erizo::dataPacket>(0, buf, len, erizo::OTHER_PACKET);
    delete pli;
    return packet;
  }
};


class HandlerTest : public ::testing::Test {
 public:
  HandlerTest() {}

 protected:
  virtual void SetUp() {
    scheduler = std::make_shared<Scheduler>(1);
    worker = std::make_shared<Worker>(scheduler);
    worker->start();
    connection = std::make_shared<erizo::MockWebRtcConnection>(worker, ice_config, rtp_maps);

    connection->setVideoSinkSSRC(erizo::kVideoSsrc);
    connection->setAudioSinkSSRC(erizo::kAudioSsrc);
    connection->setVideoSourceSSRC(erizo::kVideoSsrc);
    connection->setAudioSourceSSRC(erizo::kAudioSsrc);

    pipeline = Pipeline::create();
    reader = std::make_shared<erizo::Reader>();
    writer = std::make_shared<erizo::Writer>();

    pipeline->addBack(writer);
    setHandler();
    pipeline->addBack(reader);
    pipeline->finalize();
  }

  virtual void TearDown() {
  }

  virtual void setHandler() = 0;

  IceConfig ice_config;
  std::vector<RtpMap> rtp_maps;
  std::shared_ptr<erizo::MockWebRtcConnection> connection;
  Pipeline::Ptr pipeline;
  std::shared_ptr<erizo::Reader> reader;
  std::shared_ptr<erizo::Writer> writer;
  std::shared_ptr<Worker> worker;
  std::shared_ptr<Scheduler> scheduler;
  std::queue<std::shared_ptr<dataPacket>> packet_queue;
};

}  // namespace erizo

#endif  // ERIZO_SRC_TEST_UTILS_TOOLS_H_
