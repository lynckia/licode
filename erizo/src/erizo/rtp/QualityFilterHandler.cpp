#include "rtp/QualityFilterHandler.h"

#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(QualityFilterHandler, "rtp.QualityFilterHandler");

QualityFilterHandler::QualityFilterHandler()
  : connection_{nullptr}, enabled_{true}, initialized_{false},
  receiving_multiple_ssrc_{false}, target_spatial_layer_{0}, target_temporal_layer_{0},
  video_sink_ssrc_{0}, video_source_ssrc_{0}, last_ssrc_received_{0},
  max_video_bw_{0} {}

void QualityFilterHandler::enable() {
  enabled_ = true;
}

void QualityFilterHandler::disable() {
  enabled_ = false;
}

void QualityFilterHandler::handleFeedbackPackets(std::shared_ptr<dataPacket> packet) {
  RtpUtils::forEachRRBlock(packet, [this](RtcpHeader *chead) {
    RtpUtils::updateREMB(chead, max_video_bw_);

    RtpUtils::forEachNack(chead, [this, chead](uint16_t seq_num, uint16_t plb) {
      SequenceNumber result = translator_.reverse(seq_num);
      if (result.type == SequenceNumberType::Valid) {
        chead->setNackPid(result.input);
      }
    });
  });
}

void QualityFilterHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  if (enabled_) {
    handleFeedbackPackets(packet);  // TODO(javier) remove this line when RTCP termination is enabled

    // TODO(javier): Handle RRs and NACKs and translate Sequence Numbers?
  }

  ctx->fireRead(packet);
}

void QualityFilterHandler::checkLayers() {
  int new_spatial_layer = quality_manager_->getSpatialLayer();
  if (new_spatial_layer != target_spatial_layer_) {
    sendPLI();
    target_spatial_layer_ = new_spatial_layer;
  }
  int new_temporal_layer = quality_manager_->getTemporalLayer();
  target_temporal_layer_ = new_temporal_layer;
}

void QualityFilterHandler::checkSSRCChange(uint32_t ssrc) {
  if (last_ssrc_received_ != ssrc) {
    translator_.reset();
  }
  last_ssrc_received_ = ssrc;
}

void QualityFilterHandler::sendPLI() {
  getContext()->fireRead(RtpUtils::createPLI(video_sink_ssrc_, video_source_ssrc_));
}

void QualityFilterHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);

    checkLayers();

    uint32_t ssrc = rtp_header->getSSRC();
    uint16_t sequence_number = rtp_header->getSeqNumber();

    if (ssrc != last_ssrc_received_) {
      receiving_multiple_ssrc_ = true;
    }

    if (!packet->belongsToSpatialLayer(target_spatial_layer_)) {
      if (ssrc == video_sink_ssrc_) {
        translator_.get(sequence_number, true);
      }
      return;
    }

    checkSSRCChange(ssrc);
    rtp_header->setSSRC(video_sink_ssrc_);

    if (!packet->belongsToTemporalLayer(target_temporal_layer_)) {
      translator_.get(sequence_number, true);
      return;
    }

    SequenceNumber sequence_number_info = translator_.get(sequence_number, false);
    if (sequence_number_info.type != SequenceNumberType::Valid) {
      return;
    }

    if (packet->compatible_spatial_layers.back() == target_spatial_layer_ && packet->ending_of_layer_frame) {
      rtp_header->setMarker(1);
    }

    rtp_header->setSeqNumber(sequence_number_info.output);
  }

  // TODO(javier): Handle SRs and translate Sequence Numbers?

  ctx->fireWrite(packet);
}

void QualityFilterHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  auto processor = pipeline->getService<RtcpProcessor>();

  if (processor) {
    max_video_bw_ = processor->getMaxVideoBW();
  }

  if (initialized_) {
    return;
  }

  connection_ = pipeline->getService<WebRtcConnection>().get();
  if (!connection_) {
    return;
  }

  quality_manager_ = pipeline->getService<QualityManager>();

  video_sink_ssrc_ = connection_->getVideoSinkSSRC();
  video_source_ssrc_ = connection_->getVideoSourceSSRC();
}
}  // namespace erizo
