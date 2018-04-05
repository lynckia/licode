#include "rtp/QualityFilterHandler.h"

#include "./MediaStream.h"
#include "lib/ClockUtils.h"
#include "rtp/RtpUtils.h"
#include "rtp/RtpVP8Parser.h"

namespace {

struct Ntp_timestamp {
  uint32_t msw = 0;
  uint32_t lsw = 0;
};

constexpr double kNtpFracPerMs = 4.294967296E6;

int64_t ntpToMs(Ntp_timestamp ts) {
  const double ntp_frac_ms = static_cast<double>(ts.lsw) / kNtpFracPerMs;
  return 1000 * static_cast<int64_t>(ts.msw) +
         static_cast<int64_t>(ntp_frac_ms + 0.5);
}

}  // namespace

namespace erizo {

DEFINE_LOGGER(QualityFilterHandler, "rtp.QualityFilterHandler");

constexpr duration kSwitchTimeout = std::chrono::seconds(3);

QualityFilterHandler::QualityFilterHandler()
    : stream_{nullptr},
      enabled_{true},
      initialized_{false},
      receiving_multiple_ssrc_{false},
      changing_spatial_layer_{false},
      is_scalable_{false},
      target_spatial_layer_{0},
      future_spatial_layer_{-1},
      target_temporal_layer_{0},
      video_sink_ssrc_{0},
      video_source_ssrc_{0},
      last_ssrc_received_{0},
      max_video_bw_{0},
      last_timestamp_sent_{0},
      timestamp_offset_{0},
      time_change_started_{clock::now()},
      picture_id_offset_{0},
      last_picture_id_sent_{0} {}

void QualityFilterHandler::enable() { enabled_ = true; }

void QualityFilterHandler::disable() { enabled_ = false; }

void QualityFilterHandler::handleFeedbackPackets(
    const std::shared_ptr<DataPacket> &packet) {
  RtpUtils::forEachRRBlock(packet, [this](RtcpHeader *chead) {
    if (chead->packettype == RTCP_PS_Feedback_PT &&
        (chead->getBlockCount() == RTCP_PLI_FMT ||
         chead->getBlockCount() == RTCP_SLI_FMT ||
         chead->getBlockCount() == RTCP_PLI_FMT)) {
      sendPLI();
    }
  });
}

void QualityFilterHandler::read(Context *ctx,
                                std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader *>(packet->data);
  if (chead->isFeedback() && enabled_ && is_scalable_) {
    handleFeedbackPackets(packet);
    return;
  }

  ctx->fireRead(std::move(packet));
}

void QualityFilterHandler::checkLayers() {
  int new_spatial_layer = quality_manager_->getSpatialLayer();
  if (new_spatial_layer != target_spatial_layer_) {
    sendPLI();
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

void QualityFilterHandler::sendPLI() {
  getContext()->fireRead(
      RtpUtils::createPLI(video_sink_ssrc_, video_source_ssrc_));
}

void QualityFilterHandler::changeSpatialLayerOnKeyframeReceived(
    const std::shared_ptr<DataPacket> &packet) {
  if (future_spatial_layer_ == -1) {
    return;
  }

  time_point now = clock::now();

  if (packet->belongsToSpatialLayer(future_spatial_layer_) &&
      packet->belongsToTemporalLayer(target_temporal_layer_) &&
      packet->is_keyframe) {
    target_spatial_layer_ = future_spatial_layer_;
    future_spatial_layer_ = -1;
  } else if (now - time_change_started_ > kSwitchTimeout) {
    sendPLI();
    target_spatial_layer_ = future_spatial_layer_;
    future_spatial_layer_ = -1;
  }
}

void QualityFilterHandler::detectVideoScalability(
    const std::shared_ptr<DataPacket> &packet) {
  if (is_scalable_ || packet->type != VIDEO_PACKET) {
    return;
  }
  if (packet->belongsToTemporalLayer(1) || packet->belongsToSpatialLayer(1)) {
    is_scalable_ = true;
    quality_manager_->enable();
  }
}

void QualityFilterHandler::updatePictureID(
    const std::shared_ptr<DataPacket> &packet) {
  if (packet->codec == "VP8") {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader *>(packet->data);
    unsigned char *start_buffer =
        reinterpret_cast<unsigned char *>(packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    RtpVP8Parser::setVP8PictureID(
        start_buffer, packet->length - rtp_header->getHeaderLength(),
        last_picture_id_sent_);
  }
}

void QualityFilterHandler::write(Context *ctx,
                                 std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader *>(packet->data);

  if (chead->isRtcp()) {
    if (chead->packettype == RTCP_Sender_PT) {
      if (last_rtcp_timestamp.find(chead->getSSRC()) ==
          last_rtcp_timestamp.end()) {
        ELOG_DEBUG("Add first rtcp rtp ts for SSRC: %u", chead->getSSRC());
      }
      last_rtcp_timestamp[chead->getSSRC()] = chead->getRtpTimestamp();
      ntp_ms[chead->getSSRC()] =
          ntpToMs({chead->getNtpTimestampMSW(), chead->getNtpTimestampLSW()});
    }
  }

  detectVideoScalability(packet);

  if (is_scalable_ && !chead->isRtcp() && enabled_ &&
      packet->type == VIDEO_PACKET) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader *>(packet->data);

    checkLayers();

    uint32_t ssrc = rtp_header->getSSRC();
    uint16_t sequence_number = rtp_header->getSeqNumber();
    int picture_id = packet->picture_id;

    if (last_ssrc_received_ != 0 && ssrc != last_ssrc_received_) {
      receiving_multiple_ssrc_ = true;
    }

    changeSpatialLayerOnKeyframeReceived(packet);

    if (!packet->belongsToSpatialLayer(target_spatial_layer_)) {
      if (!receiving_multiple_ssrc_) {
        translator_.get(sequence_number, true);
      }
      return;
    }

    uint32_t new_timestamp = rtp_header->getTimestamp();

    if (checkSSRCChange(ssrc)) {
      translator_.reset();
      if (last_picture_id_sent_ > 0) {
        picture_id_offset_ = last_picture_id_sent_ - picture_id + 1;
      }
    }

    // Keep aligned with SRs
    if (last_timestamp_sent_ > 0) {
      if (base_ts_ssrc != ssrc) {
        if (last_rtcp_timestamp.find(base_ts_ssrc) !=
                last_rtcp_timestamp.end() &&
            last_rtcp_timestamp.find(ssrc) != last_rtcp_timestamp.end()) {
          timestamp_offset_ = last_rtcp_timestamp[base_ts_ssrc] -
                              last_rtcp_timestamp[ssrc] +
                              1;  // Calculate offset based on rtcp SR
          int64_t rebase = (ntp_ms[base_ts_ssrc] - ntp_ms[ssrc]) *
                           (packet->clock_rate / 1000);
          timestamp_offset_ = timestamp_offset_ - rebase;
        } else {
          timestamp_offset_ = last_timestamp_sent_ - new_timestamp +
                              1;  // Original "poor" implementation. If there's
                                  // a lag from the publisher, the sync goes out
        }
      } else {
        timestamp_offset_ = 0;  // Don't need offset
      }
    }

    if (!packet->belongsToTemporalLayer(target_temporal_layer_)) {
      translator_.get(sequence_number, true);
      return;
    }

    SequenceNumber sequence_number_info =
        translator_.get(sequence_number, false);
    if (sequence_number_info.type != SequenceNumberType::Valid) {
      return;
    }

    if (packet->compatible_spatial_layers.back() == target_spatial_layer_ &&
        packet->ending_of_layer_frame) {
      rtp_header->setMarker(1);
    }

    rtp_header->setSSRC(video_sink_ssrc_);
    rtp_header->setSeqNumber(sequence_number_info.output);

    // Video frames are split in multiple packets. If we don't do this check and
    // the new keyframe has ts < last sent ts, each packet of the same frame
    // would have an increasing +1 ts. Firefox discard this packet resulting in
    // a freeze or quality loss. Instead we set the same ts for all packets
    // belonging to the same video frame
    if (rtp_header->getTimestamp() != last_input_ts) {
      // Avoid sending negative ts
      if (last_timestamp_sent_ > (new_timestamp + timestamp_offset_)) {
        last_timestamp_sent_ = last_timestamp_sent_ + 1;
      } else {
        last_timestamp_sent_ = new_timestamp + timestamp_offset_;
      }
    }

    last_input_ts = rtp_header->getTimestamp();

    rtp_header->setTimestamp(last_timestamp_sent_);

    last_picture_id_sent_ = picture_id + picture_id_offset_;
    updatePictureID(packet);

    if (base_ts_ssrc == 0) {
      base_ts_ssrc = ssrc;
    }
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
