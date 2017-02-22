#ifndef ERIZO_SRC_ERIZO_RTP_RTCPNACKGENERATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPNACKGENERATOR_H_

#include <memory>
#include <string>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"

#define MAX_DELAY 450000

namespace erizo {

class WebRtcConnection;


class NackInfo {
 public:
  NackInfo(): seq_num{0}, retransmits{0} {}
  explicit NackInfo(uint16_t seq_num): seq_num{seq_num}, retransmits{0} {}
  uint16_t seq_num;
  uint16_t retransmits;
};

class RtcpNackGenerator{
  DECLARE_LOGGER();

 public:
  explicit RtcpNackGenerator(uint32_t ssrc_);
  bool handleRtpPacket(std::shared_ptr<dataPacket> packet);
  std::shared_ptr<dataPacket> addNackPacketToRr(std::shared_ptr<dataPacket> rr_packet);

 private:
  bool rtpSequenceLessThan(uint16_t x, uint16_t y);
  bool addNacks(uint16_t seq_num);

 private:
  uint8_t packet_[128];
  uint16_t highest_seq_num_;
  uint32_t ssrc_;
  NackInfo nack_info_;
  std::vector<NackInfo> nack_info_list_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPNACKGENERATOR_H_
