#include "./MediaDefinitions.h"
#include "rtp/TemporalLayerSwitchHandler.h"

namespace erizo {

DEFINE_LOGGER(TemporalLayerSwitchHandler, "rtp.TemporalLayerSwitchHandler");

TemporalLayerSwitchHandler::TemporalLayerSwitchHandler() : connection_{nullptr}, last_sent_seq_num_{0} {}


void TemporalLayerSwitchHandler::enable() {
}

void TemporalLayerSwitchHandler::disable() {
}

void TemporalLayerSwitchHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
  }
}

void TemporalLayerSwitchHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);

  if (packet->type != VIDEO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireWrite(packet);
    return;
  }

  RtpMap *codec = connection_->getRemoteSdpInfo().getCodecByExternalPayloadType(rtp_header->getPayloadType());
  if (codec && codec->encoding_name == "VP8" && !isFrameInLayer(packet, 2, 2000)) {
    return;
  }

  if (last_sent_seq_num_ != -1) {
    rtp_header->setSeqNumber(last_sent_seq_num_ + 1);
  }
  last_sent_seq_num_ = rtp_header->getSeqNumber();

  rtp_header->setSSRC(connection_->getVideoSinkSSRC());
  ctx->fireWrite(packet);
}

void TemporalLayerSwitchHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  int len = packet->length;
  if (chead->isFeedback()) {
    char* movingBuf = packet->data;
    int rtcpLength = 0;
    int totalLength = 0;
    int currentBlock = 0;

    do {
      movingBuf+=rtcpLength;
      chead = reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength = (ntohs(chead->length) + 1) * 4;
      totalLength += rtcpLength;
      switch (chead->packettype) {
        case RTCP_PS_Feedback_PT:
          //            ELOG_DEBUG("RTCP PS FB TYPE: %u", chead->getBlockCount() );
          switch (chead->getBlockCount()) {
            case RTCP_AFB:
              {
                char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
                if (!strncmp(uniqueId, "REMB", 4)) {
                  chead->setREMBBitRate(450000);
                }
                break;
              }
            default:
              break;
          }
        default:
          break;
      }
      currentBlock++;
    } while (totalLength < len);
  }
  ctx->fireRead(packet);
}

bool TemporalLayerSwitchHandler::isFrameInLayer(std::shared_ptr<dataPacket> packet, int temporal_layer,
        unsigned int ssrc) {
  bool is_frame_in_temp_layer = false;
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadVP8* payload = vp8_parser_.parseVP8(
      start_buffer, packet->length - rtp_header->getHeaderLength());
  if (!payload->hasTID || payload->tID <= temporal_layer) {
    is_frame_in_temp_layer = true;
  }
  if (rtp_header->getSSRC() != ssrc) {
    is_frame_in_temp_layer = false;
  }
  delete payload;
  return is_frame_in_temp_layer;
}

}  // namespace erizo
