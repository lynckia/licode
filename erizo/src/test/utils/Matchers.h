#ifndef ERIZO_SRC_TEST_UTILS_MATCHERS_H_
#define ERIZO_SRC_TEST_UTILS_MATCHERS_H_

#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>

namespace erizo {

MATCHER_P(RtpHasPayloadType, pt, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getPayloadType() == pt;
}

MATCHER_P(RtpHasSsrc, ssrc, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getSSRC() == ssrc;
}

MATCHER_P(RtpHasSequenceNumberFromBuffer, seq_num, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getSeqNumber() == seq_num;
}

MATCHER_P(RtpHasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)->data))->getSeqNumber() == seq_num;
}

MATCHER_P(NackHasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getNackPid() == seq_num;
}

MATCHER_P(ReceiverReportHasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getHighestSeqnum() == seq_num;
}

MATCHER_P(RembHasBitrateValue, bitrate, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getREMBBitRate() == bitrate;
}

MATCHER(IsKeyframeFirstPacket, "") {
  erizo::RtpHeader *packet = reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg));
  char* data_pointer;
  char* parsing_pointer;
  data_pointer = std::get<0>(arg) + packet->getHeaderLength();
  parsing_pointer = data_pointer;
  if (*parsing_pointer != 0x10) {
    return false;
  }
  parsing_pointer++;
  if (*parsing_pointer == 0x00) {
    return true;
  }
  return false;
}

}  // namespace erizo

#endif  // ERIZO_SRC_TEST_UTILS_MATCHERS_H_
