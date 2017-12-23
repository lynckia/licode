#include "rtp/RtcpProcessorHandler.h"
#include "./MediaDefinitions.h"
#include "./MediaStream.h"

namespace erizo {

DEFINE_LOGGER(RtcpProcessorHandler, "rtp.RtcpProcessorHandler");

RtcpProcessorHandler::RtcpProcessorHandler() : stream_{nullptr} {
}

void RtcpProcessorHandler::enable() {
}

void RtcpProcessorHandler::disable() {
}

void RtcpProcessorHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    if (chead->packettype == RTCP_Sender_PT) {  // Sender Report
      processor_->analyzeSr(chead);
    }
  } else {
    if (stats_->getNode()["total"].hasChild("bitrateCalculated")) {
       processor_->setPublisherBW(stats_->getNode()["total"]["bitrateCalculated"].value());
    }
  }
  processor_->checkRtcpFb();
  ctx->fireRead(std::move(packet));
}

void RtcpProcessorHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (chead->isFeedback()) {
    int length = processor_->analyzeFeedback(packet->data, packet->length);
    if (length) {
      ctx->fireWrite(std::move(packet));
    }
    return;
  }
  ctx->fireWrite(std::move(packet));
}

void RtcpProcessorHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
    processor_ = pipeline->getService<RtcpProcessor>();
    stats_ = pipeline->getService<Stats>();
  }
}
}  // namespace erizo
