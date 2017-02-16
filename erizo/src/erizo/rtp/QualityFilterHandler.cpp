#include "rtp/QualityFilterHandler.h"

#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(QualityFilterHandler, "rtp.QualityFilterHandler");

QualityFilterHandler::QualityFilterHandler()
  : connection_{nullptr}, enabled_{true}, initialized_{false},
  last_seq_number_{0},
  target_spatial_layer_{0}, target_temporal_layer_{0}, video_sink_ssrc_{0} {}

void QualityFilterHandler::enable() {
  enabled_ = true;
}

void QualityFilterHandler::disable() {
  enabled_ = false;
}

void QualityFilterHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
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
          switch (chead->getBlockCount()) {
            case RTCP_AFB:
              {
                char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
                if (!strncmp(uniqueId, "REMB", 4)) {
                  chead->setREMBBitRate(300000);
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

void QualityFilterHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    if (!packet->belongsToSpatialLayer(target_spatial_layer_) ||
        !packet->belongsToTemporalLayer(target_temporal_layer_)) {
          return;
    }
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    rtp_header->setSSRC(video_sink_ssrc_);
    rtp_header->setSeqNumber(++last_seq_number_);
  }

  ctx->fireWrite(packet);
}

void QualityFilterHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  connection_ = pipeline->getService<WebRtcConnection>().get();
  if (!connection_) {
    return;
  }

  video_sink_ssrc_ = connection_->getVideoSinkSSRC();
}
}  // namespace erizo
