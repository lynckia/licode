#include "rtp/RtcpProcessorHandler.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

namespace erizo {

DEFINE_LOGGER(RtcpProcessorHandler, "rtp.RtcpProcessorHandler");

RtcpProcessorHandler::RtcpProcessorHandler(WebRtcConnection *connection, std::shared_ptr<RtcpProcessor> processor) :
    connection_{connection}, processor_{processor} {
}

void RtcpProcessorHandler::enable() {
}

void RtcpProcessorHandler::disable() {
}

void RtcpProcessorHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    if (chead->packettype == RTCP_Sender_PT) {  // Sender Report
      processor_->analyzeSr(chead);
    }
  } else {
    if (connection_->getStats().getLatestTotalBitrate()) {
      processor_->setPublisherBW(connection_->getStats().getLatestTotalBitrate());
    }
  }
  processor_->checkRtcpFb();
  ctx->fireRead(packet);
}

void RtcpProcessorHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (chead->isFeedback()) {
    int length = processor_->analyzeFeedback(packet->data, packet->length);
    if (length) {
      ctx->fireWrite(packet);
    }
    return;
  }
  ctx->fireWrite(packet);
}

void RtcpProcessorHandler::notifyUpdate() {
}
}  // namespace erizo
