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

void QualityFilterHandler::applyMaxRembMaybe(std::shared_ptr<dataPacket> packet) {
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
}

void QualityFilterHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  applyMaxRembMaybe(packet);  // TODO(javier) remove this line when RTCP termination is enabled

  // TODO(javier): Handle RRs and NACKs and translate Sequence Numbers?

  ctx->fireRead(packet);
}

void QualityFilterHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);

    if (!packet->belongsToSpatialLayer(target_spatial_layer_)) {
      return;
    }
    rtp_header->setSSRC(video_sink_ssrc_);

    uint16_t sequence_number = rtp_header->getSeqNumber();
    if (!packet->belongsToTemporalLayer(target_temporal_layer_)) {
      sequence_number_translator.get(sequence_number, true);
      return;
    }
    SequenceNumber sequence_number_info = sequence_number_translator.get(sequence_number, false);
    if (sequence_number_info.type != SequenceNumberType::Valid) {
      return;
    }

    rtp_header->setSeqNumber(sequence_number_info.output);
  }

  // TODO(javier): Handle SRs and translate Sequence Numbers?

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
