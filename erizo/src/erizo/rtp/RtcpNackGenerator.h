#ifndef ERIZO_SRC_ERIZO_RTP_RTCPNACKGENERATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPNACKGENERATOR_H_

#include <memory>
#include <string>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "lib/ClockUtils.h"

#define MAX_DELAY 450000

namespace erizo {

class NackInfo {
 public:
  NackInfo(): seq_num{0}, retransmits{0}, sent_time{0} {}
  explicit NackInfo(uint16_t seq_num): seq_num{seq_num}, retransmits{0}, sent_time{0} {}
  uint16_t seq_num;
  uint16_t retransmits;
  uint64_t sent_time;
};

class RtcpNackGenerator{
  DECLARE_LOGGER();

 public:
  explicit RtcpNackGenerator(uint32_t ssrc_,
      std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());
  bool handleRtpPacket(std::shared_ptr<DataPacket> packet);
  bool addNackPacketToRr(std::shared_ptr<DataPacket> rr_packet);

 private:
  bool addNacks(uint16_t seq_num);
  bool isTimeToRetransmit(const NackInfo& nack_info, uint64_t current_time_ms);

 private:
  bool initialized_;
  uint16_t highest_seq_num_;
  uint32_t ssrc_;
  NackInfo nack_info_;
  std::vector<NackInfo> nack_info_list_;
  std::shared_ptr<Clock> clock_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPNACKGENERATOR_H_
