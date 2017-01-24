#ifndef ERIZO_SRC_TEST_UTILS_MATCHERS_H_
#define ERIZO_SRC_TEST_UTILS_MATCHERS_H_

#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>

MATCHER_P(HasSequenceNumber, seq_num, "") {
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


#endif  // ERIZO_SRC_TEST_UTILS_MATCHERS_H_
