#include "rtp/RtpUtils.h"

#include <memory>

namespace erizo {

bool RtpUtils::sequenceNumberLessThan(uint16_t first, uint16_t last) {
  uint16_t result = first - last;
  return result > 0xF000;
}

void RtpUtils::updateREMB(RtcpHeader *chead, uint bitrate) {
  if (chead->packettype == RTCP_PS_Feedback_PT && chead->getBlockCount() == RTCP_AFB) {
    char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
    if (!strncmp(uniqueId, "REMB", 4)) {
      chead->setREMBBitRate(bitrate);
    }
  }
}

void RtpUtils::forEachNack(RtcpHeader *chead, std::function<void(uint16_t, uint16_t)> f) {
  if (chead->packettype == RTCP_RTP_Feedback_PT) {
    uint16_t initial_seq_num = chead->getNackPid();
    uint16_t plb = chead->getNackBlp();
    f(initial_seq_num, plb);
  }
}

std::shared_ptr<dataPacket> RtpUtils::createPLI(uint32_t source_ssrc, uint32_t sink_ssrc) {
  RtcpHeader pli;
  pli.setPacketType(RTCP_PS_Feedback_PT);
  pli.setBlockCount(1);
  pli.setSSRC(sink_ssrc);
  pli.setSourceSSRC(source_ssrc);
  pli.setLength(2);
  char *buf = reinterpret_cast<char*>(&pli);
  int len = (pli.getLength() + 1) * 4;
  return std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET);
}

void RtpUtils::forEachRRBlock(std::shared_ptr<dataPacket> packet, std::function<void(RtcpHeader*)> f) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  int len = packet->length;
  if (chead->isFeedback()) {
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

}  // namespace erizo
