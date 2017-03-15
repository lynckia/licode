#include "rtp/LayerDetectorHandler.h"

#include <vector>

#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(LayerDetectorHandler, "rtp.LayerDetectorHandler");

LayerDetectorHandler::LayerDetectorHandler() : connection_{nullptr}, enabled_{true}, initialized_{false} {}

void LayerDetectorHandler::enable() {
  enabled_ = true;
}

void LayerDetectorHandler::disable() {
  enabled_ = false;
}

void LayerDetectorHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    RtpMap *codec = connection_->getRemoteSdpInfo().getCodecByExternalPayloadType(rtp_header->getPayloadType());
    if (codec && codec->encoding_name == "VP8") {
      parseLayerInfoFromVP8(packet);
    } else if (codec && codec->encoding_name == "VP9") {
      parseLayerInfoFromVP9(packet);
    }
  }
  ctx->fireRead(packet);
}

int LayerDetectorHandler::getSsrcPosition(uint32_t ssrc) {
  std::vector<uint32_t>::iterator item = std::find(video_ssrc_list_.begin(), video_ssrc_list_.end(), ssrc);
  size_t index = std::distance(video_ssrc_list_.begin(), item);
  if (index != video_ssrc_list_.size()) {
    return index;
  }
  return -1;
}

void LayerDetectorHandler::parseLayerInfoFromVP8(std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadVP8* payload = vp8_parser_.parseVP8(
      start_buffer, packet->length - rtp_header->getHeaderLength());

  packet->compatible_temporal_layers = {};
  switch (payload->tID) {
    case 0:
      packet->compatible_temporal_layers.push_back(0);
    case 2:
      packet->compatible_temporal_layers.push_back(1);
    case 1:
      packet->compatible_temporal_layers.push_back(2);
    // case 3 and beyond are not handled because Chrome only
    // supports 3 temporal scalability today (03/15/17)
      break;
    default:
      packet->compatible_temporal_layers.push_back(0);
      break;
  }

  int position = getSsrcPosition(rtp_header->getSSRC());
  packet->compatible_spatial_layers = {position};
  if (!payload->frameType) {
    packet->is_keyframe = true;
  } else {
    packet->is_keyframe = false;
  }
  delete payload;
}

void LayerDetectorHandler::parseLayerInfoFromVP9(std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadVP9* payload = vp9_parser_.parseVP9(
      start_buffer, packet->length - rtp_header->getHeaderLength());

  int spatial_layer = payload->spatialID;

  packet->compatible_spatial_layers = {};
  for (int i = 5; i >= spatial_layer; i--) {
    packet->compatible_spatial_layers.push_back(i);
  }

  packet->compatible_temporal_layers = {};
  switch (payload->temporalID) {
    case 0:
      packet->compatible_temporal_layers.push_back(0);
    case 2:
      packet->compatible_temporal_layers.push_back(1);
    case 1:
      packet->compatible_temporal_layers.push_back(2);
    case 3:
      packet->compatible_temporal_layers.push_back(3);
      break;
    default:
      packet->compatible_temporal_layers.push_back(0);
      break;
  }

  if (!payload->frameType) {
    packet->is_keyframe = true;
  } else {
    packet->is_keyframe = false;
  }

  packet->ending_of_layer_frame = payload->endingOfLayerFrame;
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

  connection_ = pipeline->getService<WebRtcConnection>().get();
  if (!connection_) {
    return;
  }

  video_ssrc_list_ = connection_->getVideoSourceSSRCList();
}
}  // namespace erizo
