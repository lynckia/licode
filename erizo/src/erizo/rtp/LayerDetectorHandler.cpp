#include "rtp/LayerDetectorHandler.h"

#include <vector>

#include "./MediaStream.h"
#include "lib/ClockUtils.h"

namespace erizo {

static constexpr uint32_t kMaxTemporalLayers = 4;
static constexpr uint32_t kMaxSpatialLayers = 4;
static constexpr erizo::duration kMinNotifyLayerInfoInterval = std::chrono::seconds(5);

DEFINE_LOGGER(LayerDetectorHandler, "rtp.LayerDetectorHandler");

LayerDetectorHandler::LayerDetectorHandler(std::shared_ptr<erizo::Clock> the_clock)
    : clock_{the_clock}, stream_{nullptr}, enabled_{true}, initialized_{false},
    last_event_sent_{the_clock->now()} {
  video_ssrc_list_ = std::vector<uint32_t>(kMaxSpatialLayers, 0);
  video_frame_height_list_ = std::vector<uint32_t>(kMaxSpatialLayers, 0);
  video_frame_width_list_ = std::vector<uint32_t>(kMaxSpatialLayers, 0);
  video_frame_rate_list_ = std::vector<MovingIntervalRateStat>();
  for (uint32_t i = 0; i < kMaxTemporalLayers; i++) {
    video_frame_rate_list_.emplace_back(MovingIntervalRateStat{std::chrono::milliseconds(500), 10, .5, clock_});
  }
}

void LayerDetectorHandler::enable() {
  enabled_ = true;
}

void LayerDetectorHandler::disable() {
  enabled_ = false;
}

void LayerDetectorHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    if (packet->codec == "VP8") {
      parseLayerInfoFromVP8(packet);
    } else if (packet->codec == "VP9") {
      parseLayerInfoFromVP9(packet);
    } else if (packet->codec == "H264") {
      parseLayerInfoFromH264(packet);
    }
  }
  ctx->fireRead(std::move(packet));
}

void LayerDetectorHandler::notifyLayerInfoChangedEvent() {
  ELOG_DEBUG("Layer info changed");
  for (uint32_t spatial_layer = 0; spatial_layer < video_frame_width_list_.size(); spatial_layer++) {
    ELOG_DEBUG("Spatial Layer (%u): %u %u",
              spatial_layer, video_frame_width_list_[spatial_layer], video_frame_height_list_[spatial_layer]);
  }
  std::vector<uint64_t> video_frame_rate_list;
  for (uint32_t temporal_layer = 0; temporal_layer < video_frame_rate_list_.size(); temporal_layer++) {
    video_frame_rate_list.push_back(video_frame_rate_list_[temporal_layer].value());
    ELOG_DEBUG("Temporal Layer (%u): %lu",
              temporal_layer, video_frame_rate_list[temporal_layer]);
  }

  if (stream_) {
    stream_->notifyToEventSink(
      std::make_shared<LayerInfoChangedEvent>(video_frame_width_list_,
        video_frame_height_list_, video_frame_rate_list));
  }
  last_event_sent_ = clock_->now();
}

void LayerDetectorHandler::notifyLayerInfoChangedEventMaybe() {
  if (clock_->now() - last_event_sent_ > kMinNotifyLayerInfoInterval) {
    notifyLayerInfoChangedEvent();
  }
}

int LayerDetectorHandler::getSsrcPosition(uint32_t ssrc) {
  std::vector<uint32_t>::iterator item = std::find(video_ssrc_list_.begin(), video_ssrc_list_.end(), ssrc);
  size_t index = std::distance(video_ssrc_list_.begin(), item);
  if (index != video_ssrc_list_.size()) {
    return index;
  }
  return -1;
}

void LayerDetectorHandler::parseLayerInfoFromVP8(std::shared_ptr<DataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadVP8* payload = vp8_parser_.parseVP8(
      start_buffer, packet->length - rtp_header->getHeaderLength());
  if (payload->hasPictureID) {
    packet->picture_id = payload->pictureID;
  }
  if (payload->hasTl0PicIdx) {
    packet->tl0_pic_idx = payload->tl0PicIdx;
  }
  packet->compatible_temporal_layers = {};
  switch (payload->tID) {
    case 0: addTemporalLayerAndCalculateRate(packet, 0, payload->beginningOfPartition);
    case 1: addTemporalLayerAndCalculateRate(packet, 1, payload->beginningOfPartition);
    case 2: addTemporalLayerAndCalculateRate(packet, 2, payload->beginningOfPartition);
    // case 3 and beyond are not handled because Chrome only
    // supports 3 temporal scalability today (03/15/17)
      break;
    default: addTemporalLayerAndCalculateRate(packet, 0, payload->beginningOfPartition);
      break;
  }

  int position = getSsrcPosition(rtp_header->getSSRC());
  packet->compatible_spatial_layers = {position};
  if (!payload->frameType) {
    packet->is_keyframe = true;
  } else {
    packet->is_keyframe = false;
  }

  if (payload->frameWidth != -1 && static_cast<uint>(payload->frameWidth) != video_frame_width_list_[position]) {
    video_frame_width_list_[position] = payload->frameWidth;
    video_frame_height_list_[position] = payload->frameHeight;
    notifyLayerInfoChangedEvent();
  }
  notifyLayerInfoChangedEventMaybe();
  delete payload;
}

void LayerDetectorHandler::addTemporalLayerAndCalculateRate(const std::shared_ptr<DataPacket> &packet,
                                                            int temporal_layer, bool new_frame) {
  if (new_frame) {
    video_frame_rate_list_[temporal_layer]++;
  }
  packet->compatible_temporal_layers.push_back(temporal_layer);
}

void LayerDetectorHandler::parseLayerInfoFromVP9(std::shared_ptr<DataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadVP9* payload = vp9_parser_.parseVP9(
      start_buffer, packet->length - rtp_header->getHeaderLength());

  if (payload->hasPictureID) {
    packet->picture_id = payload->pictureID;
  }

  int spatial_layer = payload->spatialID;

  packet->compatible_spatial_layers = {};
  for (int i = 5; i >= spatial_layer; i--) {
    packet->compatible_spatial_layers.push_back(i);
  }

  packet->compatible_temporal_layers = {};
  switch (payload->temporalID) {
    case 0: addTemporalLayerAndCalculateRate(packet, 0, payload->beginningOfLayerFrame);
    case 2: addTemporalLayerAndCalculateRate(packet, 1, payload->beginningOfLayerFrame);
    case 1: addTemporalLayerAndCalculateRate(packet, 2, payload->beginningOfLayerFrame);
    case 3: addTemporalLayerAndCalculateRate(packet, 3, payload->beginningOfLayerFrame);
      break;
    default: addTemporalLayerAndCalculateRate(packet, 0, payload->beginningOfLayerFrame);
      break;
  }

  if (!payload->frameType) {
    packet->is_keyframe = true;
  } else {
    packet->is_keyframe = false;
  }
  bool resolution_changed = false;
  if (payload->resolutions.size() > 0) {
    for (uint position = 0; position < payload->resolutions.size(); position++) {
      resolution_changed = true;
      video_frame_width_list_[position] = payload->resolutions[position].width;
      video_frame_height_list_[position] = payload->resolutions[position].height;
    }
  }
  if (resolution_changed) {
    notifyLayerInfoChangedEvent();
  }

  notifyLayerInfoChangedEventMaybe();

  packet->ending_of_layer_frame = payload->endingOfLayerFrame;
  delete payload;
}

void LayerDetectorHandler::parseLayerInfoFromH264(std::shared_ptr<DataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadH264* payload = h264_parser_.parseH264(
      start_buffer, packet->length - rtp_header->getHeaderLength());

  int position = getSsrcPosition(rtp_header->getSSRC());
  packet->compatible_spatial_layers = {position};

  if (payload->frameType == kH264IFrame) {
    packet->is_keyframe = true;
  } else {
    packet->is_keyframe = false;
  }

  addTemporalLayerAndCalculateRate(packet, 0, payload->start_bit);

  notifyLayerInfoChangedEventMaybe();

  delete payload;
}

void LayerDetectorHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  stream_ = pipeline->getService<MediaStream>().get();
  if (!stream_) {
    return;
  }

  video_ssrc_list_ = stream_->getVideoSourceSSRCList();
}
}  // namespace erizo
