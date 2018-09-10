#ifndef ERIZO_SRC_ERIZO_RTP_RTXPACKETTRANSLATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTXPACKETTRANSLATOR_H_

#include "pipeline/Handler.h"
#include "./logger.h"

namespace erizo {
class MediaStream;

class RtxPacketTranslator : public InboundHandler {
  DECLARE_LOGGER();
 public:
  void enable() override {}
  void disable() override {}

  std::string getName() override {
     return "rtx_packet_translator";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void setRtxMap(const std::map<uint32_t, uint32_t>& input_rtx_map);
  void notifyUpdate() override {};

private:
    std::map<uint32_t, uint32_t> rtx_map;
};

}

#endif
