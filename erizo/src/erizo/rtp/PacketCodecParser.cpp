#include "rtp/PacketCodecParser.h"

#include "./MediaStream.h"

namespace erizo {

DEFINE_LOGGER(PacketCodecParser, "rtp.PacketCodecParser");

PacketCodecParser::PacketCodecParser() :
    stream_ { nullptr }, enabled_ { true }, initialized_ { false } {
}

void PacketCodecParser::enable() {
  enabled_ = true;
}

void PacketCodecParser::disable() {
  enabled_ = false;
}

void PacketCodecParser::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && enabled_) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    RtpMap *codec =
        stream_->getRemoteSdpInfo()->getCodecByExternalPayloadType(
            rtp_header->getPayloadType());
    if (codec) {
      packet->codec = codec->encoding_name;
      packet->clock_rate = codec->clock_rate;
      ELOG_DEBUG("Reading codec: %s, clock: %u", packet->codec.c_str(), packet->clock_rate);
    }
  }
  ctx->fireRead(std::move(packet));
}

void PacketCodecParser::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  stream_ = pipeline->getService<MediaStream>().get();
  if (!stream_) {
    return;
  }
}
}  // namespace erizo
