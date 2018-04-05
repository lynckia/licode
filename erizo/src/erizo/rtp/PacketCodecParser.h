#ifndef ERIZO_SRC_ERIZO_RTP_PACKETCODECPARSER_H_
#define ERIZO_SRC_ERIZO_RTP_PACKETCODECPARSER_H_

#include "./logger.h"
#include "pipeline/Handler.h"

namespace erizo {

class MediaStream;

class PacketCodecParser: public InboundHandler {
  DECLARE_LOGGER();


 public:
  PacketCodecParser();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "packet_codec_parser";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  MediaStream *stream_;
  bool enabled_;
  bool initialized_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_PACKETCODECPARSER_H_
