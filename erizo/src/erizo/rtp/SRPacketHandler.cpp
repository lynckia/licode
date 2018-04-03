#include "rtp/SRPacketHandler.h"
#include "./MediaStream.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(SRPacketHandler, "rtp.SRPacketHandler");

SRPacketHandler::SRPacketHandler() :
    enabled_{true}, initialized_{false}, stream_(nullptr) {}


void SRPacketHandler::enable() {
  enabled_ = true;
}

void SRPacketHandler::disable() {
  enabled_ = false;
}


void SRPacketHandler::handleRtpPacket(std::shared_ptr<DataPacket> packet) {
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  uint32_t ssrc = head->getSSRC();
  auto sr_selected_info_iter = sr_info_map_.find(ssrc);
  std::shared_ptr<SRInfo> selected_info;
  if (sr_selected_info_iter == sr_info_map_.end()) {
    ELOG_DEBUG("message: Inserting new SSRC in sr_info_map, ssrc: %u", ssrc);
    sr_info_map_[ssrc] = std::make_shared<SRInfo>();
  }
  selected_info = sr_info_map_[ssrc];
  selected_info->sent_packets++;
  selected_info->sent_octets += (packet->length - head->getHeaderLength());
}



void SRPacketHandler::handleSR(std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  uint32_t ssrc = chead->getSSRC();
  auto sr_selected_info_iter = sr_info_map_.find(ssrc);
  if (sr_selected_info_iter == sr_info_map_.end()) {
    ELOG_DEBUG("message: handleSR no info for this SSRC, ssrc: %u", ssrc);
    return;
  }
  std::shared_ptr<SRInfo> selected_info = sr_selected_info_iter->second;
  ELOG_DEBUG("message: Rewriting SR, ssrc: %u, octets_sent_before: %u, packets_sent_before: %u"
    " octets_sent_after %u packets_sent_after: %u", ssrc, chead->getOctetsSent(), chead->getPacketsSent(),
    selected_info->sent_octets, selected_info->sent_packets);
  chead->setOctetsSent(selected_info->sent_octets);
  chead->setPacketsSent(selected_info->sent_packets);
}

void SRPacketHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  if (initialized_ && enabled_) {
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
    if (!chead->isRtcp() && enabled_) {
      handleRtpPacket(packet);
    } else if (chead->packettype == RTCP_Sender_PT && enabled_) {
      handleSR(packet);
    }
  }
  ctx->fireWrite(std::move(packet));
}

void SRPacketHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireRead(std::move(packet));
}

void SRPacketHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
  }
  if (!stream_) {
    return;
  }
  initialized_ = true;
}

}  // namespace erizo
