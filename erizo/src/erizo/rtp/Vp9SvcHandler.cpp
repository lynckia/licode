#include "rtp/Vp9SvcHandler.h"

#include <string>
#include <sstream>

#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

#include "rtp/RtpVP8Parser.h"

namespace erizo {

DEFINE_LOGGER(Vp9SvcHandler, "rtp.Vp9SvcHandler");

Vp9SvcHandler::Vp9SvcHandler(WebRtcConnection *connection) :
  last_sent_seq_num_{-1}, seq_num_offset_{0},
  mute_is_active_{false}, temporal_id_{0}, spatial_id_{0}, connection_{connection} {}


void Vp9SvcHandler::enable() {
}

void Vp9SvcHandler::disable() {
}

void Vp9SvcHandler::notifyUpdate() {
}

void Vp9SvcHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);
  if (packet->type != VIDEO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireRead(packet);
    return;
  }
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();

  RtpVP9Parser parser;
  RTPPayloadVP9* payload = parser.parseVP9(
      start_buffer, packet->length - rtp_header->getHeaderLength());

  bool isKeyframe = !payload->interPicturePrediction;
  spatial_id_ = 1;
  temporal_id_ = 2;
  if (payload->spatialID > spatial_id_ || payload->temporalID == 1) {
    return;
  }
  if (payload->spatialID == spatial_id_ && payload->endingOfLayerFrame) {
    rtp_header->setMarker(1);
  }

  logPayload(payload);

  if (last_sent_seq_num_ != -1) {
    rtp_header->setSeqNumber(last_sent_seq_num_ + 1);
  }
  last_sent_seq_num_ = rtp_header->getSeqNumber();
  if (last_sent_seq_num_ % 200 == 0) {
    spatial_id_++;
    connection_->sendPLI();
    spatial_id_ = spatial_id_ % 2;
  }
  ctx->fireRead(packet);
}

void Vp9SvcHandler::logPayload(RTPPayloadVP9 *payload) {
  if (true || payload->hasScalabilityStructure) {
    std::stringstream a;
    a << " ";
    for (int i = 0; i <= payload->spatialLayers; i++) {
      a << "{width:" << payload->resolutions[i].width << ",height:" << payload->resolutions[i].height << "}, ";
    }
    ELOG_WARN("VP9 packet: I(%d), P(%d), L(%d), F(%d), pictureID(%d), E(%d), spatialID(%d), temporalID(%d), %s",
                        payload->hasPictureID, payload->interPicturePrediction, payload->hasLayerIndices, payload->flexibleMode,  payload->pictureID, payload->endingOfLayerFrame, payload->spatialID, payload->temporalID, a.str().c_str());
  }
}

void Vp9SvcHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
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
  ctx->fireWrite(packet);
}

}  // namespace erizo
