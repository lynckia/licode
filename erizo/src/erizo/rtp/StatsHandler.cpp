#include "rtp/StatsHandler.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

namespace erizo {

DEFINE_LOGGER(IncomingStatsHandler, "rtp.IncomingStatsHandler");
DEFINE_LOGGER(OutgoingStatsHandler, "rtp.OutgoingStatsHandler");

IncomingStatsHandler::IncomingStatsHandler(WebRtcConnection *connection) :
  connection_{connection} {}


void IncomingStatsHandler::enable() {
}

void IncomingStatsHandler::disable() {
}

void IncomingStatsHandler::notifyUpdate() {
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

OutgoingStatsHandler::OutgoingStatsHandler(WebRtcConnection *connection) :
  connection_{connection} {}

void OutgoingStatsHandler::enable() {
}

void OutgoingStatsHandler::disable() {
}

void OutgoingStatsHandler::notifyUpdate() {
}

void OutgoingStatsHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  connection_->getStats().processRtpPacket(packet->data, packet->length);
  ctx->fireWrite(packet);
}

}  // namespace erizo
