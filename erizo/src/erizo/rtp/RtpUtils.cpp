#include "rtp/RtpUtils.h"

#include <cmath>
#include <memory>

namespace erizo {


constexpr int kMaxPacketSize = 1500;
bool RtpUtils::sequenceNumberLessThan(uint16_t first, uint16_t last) {
  return RtpUtils::numberLessThan(first, last, 16);
}

bool RtpUtils::numberLessThan(uint16_t first, uint16_t last, int bits) {
  uint16_t result = first - last;
  uint16_t mark = std::pow(2, bits) - 1;
  result = result & mark;
  uint16_t threshold = (bits > 4) ? std::pow(2, bits - 4) - 1 : std::pow(2, bits) - 1;
  return result > threshold;
}

void RtpUtils::updateREMB(RtcpHeader *chead, uint bitrate) {
  if (chead->packettype == RTCP_PS_Feedback_PT && chead->getBlockCount() == RTCP_AFB) {
    char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
    if (!strncmp(uniqueId, "REMB", 4)) {
      chead->setREMBBitRate(bitrate);
    }
  }
}

void RtpUtils::forEachNack(RtcpHeader *chead, std::function<void(uint16_t, uint16_t, RtcpHeader*)> f) {
  if (chead->packettype == RTCP_RTP_Feedback_PT) {
    int length = (chead->getLength() + 1)*4;
    int current_position = kNackCommonHeaderLengthBytes;
    uint8_t* aux_pointer = reinterpret_cast<uint8_t*>(chead);
    RtcpHeader* aux_chead;
    while (current_position < length) {
      aux_chead = reinterpret_cast<RtcpHeader*>(aux_pointer);
      uint16_t initial_seq_num = aux_chead->getNackPid();
      uint16_t plb = aux_chead->getNackBlp();
      f(initial_seq_num, plb, aux_chead);
      current_position += 4;
      aux_pointer += 4;
    }
  }
}

bool RtpUtils::isPLI(std::shared_ptr<DataPacket> packet) {
  bool is_pli = false;
  forEachRtcpBlock(packet, [&is_pli] (RtcpHeader *header) {
    if (header->getPacketType() == RTCP_PS_Feedback_PT &&
        header->getBlockCount() == RTCP_PLI_FMT) {
          is_pli = true;
        }
  });
  return is_pli;
}

bool RtpUtils::isFIR(std::shared_ptr<DataPacket> packet) {
  bool is_fir = false;
  forEachRtcpBlock(packet, [&is_fir] (RtcpHeader *header) {
    if (header->getPacketType() == RTCP_PS_Feedback_PT &&
        header->getBlockCount() == RTCP_FIR_FMT) {
          is_fir = true;
        }
  });
  return is_fir;
}

std::shared_ptr<DataPacket> RtpUtils::createPLI(uint32_t source_ssrc, uint32_t sink_ssrc,
    packetPriority priority) {
  RtcpHeader pli;
  pli.setPacketType(RTCP_PS_Feedback_PT);
  pli.setBlockCount(RTCP_PLI_FMT);
  pli.setSSRC(sink_ssrc);
  pli.setSourceSSRC(source_ssrc);
  pli.setLength(2);
  char *buf = reinterpret_cast<char*>(&pli);
  int len = (pli.getLength() + 1) * 4;
  auto packet = std::make_shared<DataPacket>(0, buf, len, VIDEO_PACKET);
  packet->priority = priority;
  return packet;
}

std::shared_ptr<DataPacket> RtpUtils::createFIR(uint32_t source_ssrc, uint32_t sink_ssrc, uint8_t seq_number) {
  RtcpHeader fir;
  fir.setPacketType(RTCP_PS_Feedback_PT);
  fir.setBlockCount(RTCP_FIR_FMT);
  fir.setSSRC(sink_ssrc);
  fir.setSourceSSRC(source_ssrc);
  fir.setLength(4);
  fir.setFIRSourceSSRC(source_ssrc);
  fir.setFIRSequenceNumber(seq_number);
  char *buf = reinterpret_cast<char*>(&fir);
  int len = (fir.getLength() + 1) * 4;
  return std::make_shared<DataPacket>(0, buf, len, VIDEO_PACKET);
}

std::shared_ptr<DataPacket> RtpUtils::createREMB(uint32_t ssrc, std::vector<uint32_t> ssrc_list, uint32_t bitrate) {
  erizo::RtcpHeader remb;
  remb.setPacketType(RTCP_PS_Feedback_PT);
  remb.setBlockCount(RTCP_AFB);
  memcpy(&remb.report.rembPacket.uniqueid, "REMB", 4);

  remb.setSSRC(ssrc);
  remb.setSourceSSRC(0);
  remb.setLength(4 + ssrc_list.size());
  remb.setREMBBitRate(bitrate);
  remb.setREMBNumSSRC(ssrc_list.size());
  uint8_t index = 0;
  for (uint32_t feed_ssrc : ssrc_list) {
    remb.setREMBFeedSSRC(index++, feed_ssrc);
  }
  int len = (remb.getLength() + 1) * 4;
  char *buf = reinterpret_cast<char*>(&remb);
  return std::make_shared<erizo::DataPacket>(0, buf, len, erizo::OTHER_PACKET);
}

std::shared_ptr<DataPacket> RtpUtils::createReceiverReport(uint32_t ssrc, uint8_t fraction_lost) {
  erizo::RtcpHeader rr;
  rr.setPacketType(RTCP_Receiver_PT);
  rr.setBlockCount(1);

  rr.setSSRC(0);
  rr.setSourceSSRC(ssrc);
  rr.setLength(8);
  rr.setFractionLost(fraction_lost);
  int len = (rr.getLength() + 1) * 4;
  char *buf = reinterpret_cast<char*>(&rr);
  return std::make_shared<erizo::DataPacket>(0, buf, len, erizo::OTHER_PACKET);
}

int RtpUtils::getPaddingLength(std::shared_ptr<DataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  if (rtp_header->hasPadding()) {
    return packet->data[packet->length - 1] & 0xFF;
  }
  return 0;
}

void RtpUtils::forEachRtcpBlock(std::shared_ptr<DataPacket> packet, std::function<void(RtcpHeader*)> f) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  int len = packet->length;
  if (chead->isRtcp()) {
    char* moving_buffer = packet->data;
    int rtcp_length = 0;
    int total_length = 0;
    int currentBlock = 0;

    do {
      moving_buffer += rtcp_length;
      chead = reinterpret_cast<RtcpHeader*>(moving_buffer);
      rtcp_length = (ntohs(chead->length) + 1) * 4;
      total_length += rtcp_length;
      f(chead);
      currentBlock++;
    } while (total_length < len);
  }
}

std::shared_ptr<DataPacket> RtpUtils::makePaddingPacket(std::shared_ptr<DataPacket> packet, uint8_t padding_size) {
  erizo::RtpHeader *header = reinterpret_cast<RtpHeader*>(packet->data);

  uint16_t packet_length = header->getHeaderLength() + padding_size;

  char packet_buffer[kMaxPacketSize];
  erizo::RtpHeader *new_header = reinterpret_cast<RtpHeader*>(packet_buffer);
  memset(packet_buffer, 0, packet_length);
  memcpy(packet_buffer, reinterpret_cast<char*>(header), header->getHeaderLength());

  new_header->setPadding(true);
  new_header->setMarker(false);
  packet_buffer[packet_length - 1] = padding_size;

  auto padding_packet = std::make_shared<DataPacket>(packet->comp, packet_buffer, packet_length, packet->type);
  padding_packet->is_padding = true;
  return padding_packet;
}

std::shared_ptr<DataPacket> RtpUtils::makeVP8BlackKeyframePacket(std::shared_ptr<DataPacket> packet) {
  uint8_t vp8_keyframe[] = {
    (uint8_t) 0x90, (uint8_t) 0xe0, (uint8_t) 0x80, (uint8_t) 0x01,  // payload header 1
    (uint8_t) 0x00, (uint8_t) 0x20, (uint8_t) 0x10, (uint8_t) 0x0f,  // payload header 2
    (uint8_t) 0x00, (uint8_t) 0x9d, (uint8_t) 0x01, (uint8_t) 0x2a,
    (uint8_t) 0x40, (uint8_t) 0x01, (uint8_t) 0xb4, (uint8_t) 0x00,
    (uint8_t) 0x07, (uint8_t) 0x07, (uint8_t) 0x09, (uint8_t) 0x03,
    (uint8_t) 0x0b, (uint8_t) 0x0b, (uint8_t) 0x11, (uint8_t) 0x33,
    (uint8_t) 0x09, (uint8_t) 0x10, (uint8_t) 0x4b, (uint8_t) 0x00,
    (uint8_t) 0x00, (uint8_t) 0x0c, (uint8_t) 0x2c, (uint8_t) 0x09,
    (uint8_t) 0xee, (uint8_t) 0x0d, (uint8_t) 0x02, (uint8_t) 0xc9,
    (uint8_t) 0x3e, (uint8_t) 0xd7, (uint8_t) 0xb7, (uint8_t) 0x36,
    (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e,
    (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70,
    (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6,
    (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e,
    (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70,
    (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6,
    (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e,
    (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70,
    (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6,
    (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e,
    (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70,
    (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6,
    (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e,
    (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70,
    (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6,
    (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e,
    (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70,
    (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6,
    (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e,
    (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70,
    (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6,
    (uint8_t) 0x4e, (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e,
    (uint8_t) 0x70, (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x70,
    (uint8_t) 0xf6, (uint8_t) 0x4e, (uint8_t) 0x5c, (uint8_t) 0x00,
    (uint8_t) 0xfe, (uint8_t) 0xef, (uint8_t) 0xb9, (uint8_t) 0x00
  };

  uint16_t keyframe_length = sizeof(vp8_keyframe)/sizeof(vp8_keyframe[0]);
  erizo::RtpHeader *header = reinterpret_cast<RtpHeader*>(packet->data);
  const uint16_t packet_length = header->getHeaderLength() + keyframe_length;
  char packet_buffer[kMaxPacketSize];

  erizo::RtpHeader *new_header = reinterpret_cast<RtpHeader*>(packet_buffer);
  memset(packet_buffer, 0, packet_length);
  memcpy(packet_buffer, reinterpret_cast<char*>(header), header->getHeaderLength());
  memcpy(packet_buffer + header->getHeaderLength(), reinterpret_cast<char*>(vp8_keyframe), keyframe_length);
  new_header->setMarker(true);
  std::shared_ptr<DataPacket> keyframe_packet =
    std::make_shared<DataPacket>(packet->comp, packet_buffer, packet_length, packet->type);
  keyframe_packet->is_keyframe = true;

  return keyframe_packet;
}

}  // namespace erizo
