#include "rtp/StatsHandler.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

namespace erizo {

DEFINE_LOGGER(IncomingStatsHandler, "rtp.IncomingStatsHandler");
DEFINE_LOGGER(OutgoingStatsHandler, "rtp.OutgoingStatsHandler");

IncomingStatsHandler::IncomingStatsHandler() : connection_{nullptr} {}


void IncomingStatsHandler::enable() {
}

void IncomingStatsHandler::disable() {
}

void IncomingStatsHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
  }
}

void IncomingStatsHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    connection_->getStats().processRtcpPacket(packet->data, packet->length);
  } else {
    connection_->getStats().processRtpPacket(packet->data, packet->length);  // Take into account ALL RTP traffic
  }

  ctx->fireRead(packet);
}

OutgoingStatsHandler::OutgoingStatsHandler() : connection_{nullptr} {}

void OutgoingStatsHandler::enable() {
}

void OutgoingStatsHandler::disable() {
}

void OutgoingStatsHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
  }
}

void OutgoingStatsHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  connection_->getStats().processRtpPacket(packet->data, packet->length);
  ctx->fireWrite(packet);
}

}  // namespace erizo
