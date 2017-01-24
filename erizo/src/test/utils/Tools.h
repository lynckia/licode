#ifndef ERIZO_SRC_TEST_UTILS_TOOLS_H_
#define ERIZO_SRC_TEST_UTILS_TOOLS_H_

#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>

#include <string>
#include <vector>

namespace erizo {

static constexpr uint16_t kVideoSsrc = 1;
static constexpr uint16_t kAudioSsrc = 2;
static constexpr uint16_t kArbitrarySeqNumber = 12;
static constexpr uint16_t kFirstSequenceNumber = 0;
static constexpr uint16_t kLastSequenceNumber = 65535;

class Tools {
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

  static std::shared_ptr<dataPacket> createVP8Packet(uint16_t seq_number, bool is_keyframe, bool is_marker) {
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
};

}  // namespace erizo

#endif  // ERIZO_SRC_TEST_UTILS_TOOLS_H_
