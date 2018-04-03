#ifndef ERIZO_SRC_ERIZO_RTP_SRPACKETHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_SRPACKETHANDLER_H_

#include <memory>
#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"

#define MAX_DELAY 450000

namespace erizo {

class MediaStream;

class SRPacketHandler: public Handler {
  DECLARE_LOGGER();


 public:
  SRPacketHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "sr_handler";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  struct SRInfo {
    SRInfo() : ssrc{0}, sent_octets{0}, sent_packets{0} {}
    uint32_t ssrc;
    uint32_t sent_octets;
    uint32_t sent_packets;
  };

  bool enabled_, initialized_;
  MediaStream* stream_;
  std::map<uint32_t, std::shared_ptr<SRInfo>> sr_info_map_;

  void handleRtpPacket(std::shared_ptr<DataPacket> packet);
  void handleSR(std::shared_ptr<DataPacket> packet);
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_SRPACKETHANDLER_H_
