#include "rtp/SRPacketHandler.h"
#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(SrPacketHandler, "rtp.SrPacketHandler");

SrPacketHandler::SrPacketHandler(WebRtcConnection *connection) :
    enabled_{true} {}


void SrPacketHandler::enable() {
  enabled_ = true;
}

void SrPacketHandler::disable() {
  enabled_ = false;
}


void SrPacketHandler::handleRtpPacket(std::shared_ptr<dataPacket> packet) {
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
  selected_info->sent_octets += packet->length - head->getHeaderLength();
}



void SrPacketHandler::handleSR(std::shared_ptr<dataPacket> packet) {
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

void SrPacketHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && enabled_) {
    handleRtpPacket(packet);
  } else if (chead->packettype == RTCP_Sender_PT && enabled_) {
    handleSR(packet);
  }
  ctx->fireRead(packet);
}

void SrPacketHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  ctx->fireRead(packet);
}

void SrPacketHandler::notifyUpdate() {
  return;
}

}  // namespace erizo
