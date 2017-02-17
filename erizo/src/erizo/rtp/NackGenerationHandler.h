#ifndef ERIZO_SRC_ERIZO_RTP_NACKGENERATIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_NACKGENERATIONHANDLER_H_

#include <memory>
#include <string>
#include <random>
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
}

class NackGenerationHandler: public Handler{
  DECLARE_LOGGER();


 public:
  NackGenerationHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "nack_generation";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  bool rtpSequenceLessThan(uint16_t x, uint16_t y);
  bool isRetransmitOfOldPacket(std::shared_ptr<dataPacket> packet, std::shared_ptr<RRPackets> rr_info);
  void handleRtpPacket(std::shared_ptr<dataPacket> packet);
  void addNacks(uint32_t ssrc, uint16_t seq_num, uint16_t highest_seq_num);
  void sendNacks(ssrc);
  uint16_t selectInterval(std::shared_ptr<RRPackets> packet_info);
  uint16_t getRandomValue(uint16_t min, uint16_t max);

 private:
  WebRtcConnection *connection_;

  uint8_t packet_[128];
  bool enabled_, initialized_;
  std::map<uint32_t, uint16_t> highest_seq_num_ssrc_map_;
  std::map<uint32_t, std::shared_ptr<std::vector<NackInfo>>> nack_info_ssrc_map_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_NACKGENERATIONHANDLER_H_
