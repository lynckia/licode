#include "rtp/QualityFilterHandler.h"

#include "./MediaStream.h"
#include "lib/ClockUtils.h"
#include "rtp/RtpUtils.h"
#include "rtp/RtpVP8Parser.h"

namespace erizo {

DEFINE_LOGGER(QualityFilterHandler, "rtp.QualityFilterHandler");

constexpr duration kSwitchTimeout = std::chrono::seconds(4);

QualityFilterHandler::QualityFilterHandler()
  : picture_id_translator_{511, 250, 15}, stream_{nullptr}, enabled_{true}, initialized_{false},
  receiving_multiple_ssrc_{false}, changing_spatial_layer_{false}, is_scalable_{false},
  target_spatial_layer_{0},
  future_spatial_layer_{-1}, target_temporal_layer_{0},
  video_sink_ssrc_{0}, video_source_ssrc_{0}, last_ssrc_received_{0},
  max_video_bw_{0}, last_timestamp_sent_{0}, timestamp_offset_{0},
  time_change_started_{clock::now()}, tl0_pic_idx_offset_{0}, last_tl0_pic_idx_sent_{0} {}

void QualityFilterHandler::enable() {
  enabled_ = true;
}

void QualityFilterHandler::disable() {
  enabled_ = false;
}

void QualityFilterHandler::handleFeedbackPackets(const std::shared_ptr<DataPacket> &packet) {
  RtpUtils::forEachRtcpBlock(packet, [this](RtcpHeader *chead) {
    if (chead->packettype == RTCP_PS_Feedback_PT &&
          (chead->getBlockCount() == RTCP_PLI_FMT ||
           chead->getBlockCount() == RTCP_SLI_FMT ||
           chead->getBlockCount() == RTCP_FIR_FMT)) {
      sendPLI();
    }
    });
}

void QualityFilterHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  // TODO(pedro): This logic drops feedback also for non-simulcast streams.
  // For clarity, we should consider using a new handler for that.
  if (chead->isFeedback() && enabled_) {
    handleFeedbackPackets(packet);
    if (!is_scalable_ && chead->isREMB()) {
      ctx->fireRead(std::move(packet));
    }
    return;
  }

  ctx->fireRead(std::move(packet));
}

void QualityFilterHandler::checkLayers() {
  int new_spatial_layer = quality_manager_->getSpatialLayer();
  if (new_spatial_layer != target_spatial_layer_ && !changing_spatial_layer_) {
    if (new_spatial_layer > target_spatial_layer_) {
      sendPLI(LOW_PRIORITY);
    } else {
      sendPLI();
    }
    future_spatial_layer_ = new_spatial_layer;
    changing_spatial_layer_ = true;
    time_change_started_ = clock::now();
  }
  int new_temporal_layer = quality_manager_->getTemporalLayer();
  target_temporal_layer_ = new_temporal_layer;
}

bool QualityFilterHandler::checkSSRCChange(uint32_t ssrc) {
  bool changed = false;
  if (last_ssrc_received_ != ssrc) {
    changed = true;
  }
  last_ssrc_received_ = ssrc;
  return changed;
}

void QualityFilterHandler::sendPLI(packetPriority priority) {
  getContext()->fireRead(RtpUtils::createPLI(video_sink_ssrc_, video_source_ssrc_, priority));
}

void QualityFilterHandler::changeSpatialLayerOnKeyframeReceived(const std::shared_ptr<DataPacket> &packet) {
  if (future_spatial_layer_ == -1) {
    return;
  }

  time_point now = clock::now();

  if (packet->belongsToSpatialLayer(future_spatial_layer_) &&
      packet->belongsToTemporalLayer(target_temporal_layer_) &&
      packet->is_keyframe) {
    target_spatial_layer_ = future_spatial_layer_;
    future_spatial_layer_ = -1;
    changing_spatial_layer_ = false;
  } else if (now - time_change_started_ > kSwitchTimeout) {
    sendPLI();
    target_spatial_layer_ = future_spatial_layer_;
    future_spatial_layer_ = -1;
    changing_spatial_layer_ = false;
  }
}

void QualityFilterHandler::detectVideoScalability(const std::shared_ptr<DataPacket> &packet) {
  if (is_scalable_ || packet->type != VIDEO_PACKET) {
    return;
  }
  if (packet->belongsToTemporalLayer(1) || packet->belongsToSpatialLayer(1)) {
    is_scalable_ = true;
    quality_manager_->enable();
  }
}

void QualityFilterHandler::updatePictureID(const std::shared_ptr<DataPacket> &packet, int new_picture_id) {
  if (packet->codec == "VP8") {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    RtpVP8Parser::setVP8PictureID(start_buffer, packet->length - rtp_header->getHeaderLength(), new_picture_id);
  }
}

void QualityFilterHandler::updateTL0PicIdx(const std::shared_ptr<DataPacket> &packet, uint8_t new_tl0_pic_idx) {
  if (packet->codec == "VP8") {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    RtpVP8Parser::setVP8TL0PicIdx(start_buffer, packet->length - rtp_header->getHeaderLength(), new_tl0_pic_idx);
  }
}

void QualityFilterHandler::removeVP8OptionalPayload(const std::shared_ptr<DataPacket> &packet) {
  if (packet->codec == "VP8") {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    int packet_length = packet->length - rtp_header->getHeaderLength();
    packet_length = RtpVP8Parser::removePictureID(start_buffer, packet_length);
    packet_length = RtpVP8Parser::removeTl0PicIdx(start_buffer, packet_length);
    packet_length = RtpVP8Parser::removeTIDAndKeyIdx(start_buffer, packet_length);
    packet->length = packet_length + rtp_header->getHeaderLength();
  }
}

void QualityFilterHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);

  detectVideoScalability(packet);

  if (is_scalable_ && !chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);

    checkLayers();

    uint32_t ssrc = rtp_header->getSSRC();
    uint16_t sequence_number = rtp_header->getSeqNumber();
    int picture_id = packet->picture_id;
    uint8_t tl0_pic_idx = packet->tl0_pic_idx;

    if (last_ssrc_received_ != 0 && ssrc != last_ssrc_received_) {
      receiving_multiple_ssrc_ = true;
    }

    changeSpatialLayerOnKeyframeReceived(packet);

    if (!packet->belongsToSpatialLayer(target_spatial_layer_)) {
      if (!receiving_multiple_ssrc_) {
        translator_.get(sequence_number, true);
        picture_id_translator_.get(picture_id, true);
      }
      return;
    }

    uint32_t new_timestamp = rtp_header->getTimestamp();

    if (checkSSRCChange(ssrc)) {
      translator_.reset();
      picture_id_translator_.reset();
      if (last_timestamp_sent_ > 0) {
        timestamp_offset_ = last_timestamp_sent_ - new_timestamp + 1;
      }
      if (last_tl0_pic_idx_sent_ > 0) {
        tl0_pic_idx_offset_ = last_tl0_pic_idx_sent_ - tl0_pic_idx + 1;
      }
    }

    if (!packet->belongsToTemporalLayer(target_temporal_layer_)) {
      translator_.get(sequence_number, true);
      picture_id_translator_.get(picture_id, true);
      return;
    }

    SequenceNumber sequence_number_info = translator_.get(sequence_number, false);
    if (sequence_number_info.type != SequenceNumberType::Valid) {
      return;
    }

    SequenceNumber picture_id_info = picture_id_translator_.get(picture_id, false);
    if (picture_id_info.type != SequenceNumberType::Valid) {
      return;
    }

    if (packet->compatible_spatial_layers.back() == target_spatial_layer_ && packet->ending_of_layer_frame) {
      rtp_header->setMarker(1);
    }

    rtp_header->setSSRC(video_sink_ssrc_);
    rtp_header->setSeqNumber(sequence_number_info.output);

    last_timestamp_sent_ = new_timestamp + timestamp_offset_;
    rtp_header->setTimestamp(last_timestamp_sent_);

    updatePictureID(packet, picture_id_info.output & 0x7FFF);

    uint8_t tl0_pic_idx_sent = tl0_pic_idx + tl0_pic_idx_offset_;
    last_tl0_pic_idx_sent_ = RtpUtils::numberLessThan(last_tl0_pic_idx_sent_, tl0_pic_idx_sent, 8) ?
      tl0_pic_idx_sent : last_tl0_pic_idx_sent_;
    updateTL0PicIdx(packet, tl0_pic_idx_sent);
    // removeVP8OptionalPayload(packet);  // TODO(javier): uncomment this line in case of issues with pictureId

  /*  TODO(pedro): Disabled as part of the hack to reduce audio drift 
   *  We will have to go back here and fix timestamp updates
  } else if (is_scalable_ && enabled_ && chead->isRtcp() && chead->isSenderReport()) {
    uint32_t ssrc = chead->getSSRC();
    if (video_sink_ssrc_ == ssrc) {
      uint32_t sr_timestamp = chead->getTimestamp();
      chead->setTimestamp(sr_timestamp + timestamp_offset_);
    }
    */
  }

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

  stream_ = pipeline->getService<MediaStream>().get();
  if (stream_) {
    video_sink_ssrc_ = stream_->getVideoSinkSSRC();
    video_source_ssrc_ = stream_->getVideoSourceSSRC();
  }

  if (initialized_) {
    return;
  }

  quality_manager_ = pipeline->getService<QualityManager>();

  initialized_ = true;
}
}  // namespace erizo
