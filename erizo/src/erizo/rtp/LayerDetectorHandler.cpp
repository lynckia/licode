#include "rtp/LayerDetectorHandler.h"

#include <vector>

#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(LayerDetectorHandler, "rtp.LayerDetectorHandler");

LayerDetectorHandler::LayerDetectorHandler()
  : connection_{nullptr}, enabled_{true}, initialized_{false}, rid_index_{-1} {}

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
    if (rid_index_ > 0) {
      parseLayerInfoFromRid(packet);
    } else if (codec && codec->encoding_name == "VP8") {
      packet->codec = "VP8";
      parseLayerInfoFromVP8(packet);
    } else if (codec && codec->encoding_name == "VP9") {
      packet->codec = "VP9";
      parseLayerInfoFromVP9(packet);
    }
  }
  ctx->fireRead(std::move(packet));
}

int LayerDetectorHandler::getSsrcPosition(uint32_t ssrc) {
  std::vector<uint32_t>::iterator item = std::find(video_ssrc_list_.begin(), video_ssrc_list_.end(), ssrc);
  size_t index = std::distance(video_ssrc_list_.begin(), item);
  if (index != video_ssrc_list_.size()) {
    return index;
  }
  return -1;
}

void LayerDetectorHandler::parseLayerInfoFromRid(std::shared_ptr<dataPacket> packet) {
  RtpHeader* head = reinterpret_cast<RtpHeader*>(packet->data);
  int position = 0;
  if (head->getExtension()) {
    uint16_t total_ext_length = head->getExtLength();
    char* ext_buffer = (char*)&head->extensions;  // NOLINT
    uint8_t ext_byte = 0;
    uint16_t current_place = 1;
    uint8_t ext_id = 0;
    uint8_t ext_length = 0;
    while (current_place < (total_ext_length * 4)) {
      ext_byte = (uint8_t)(*ext_buffer);
      ext_id = ext_byte >> 4;
      ext_length = ext_byte & 0x0F;
      if (ext_id != 0 && ext_id == rid_index_) {
        char* ptr_rid = new char[ext_length + 2];
        memcpy(ptr_rid, ext_buffer + 1, ext_length + 1);
        ptr_rid[ext_length + 1] = '\0';
        position = getSsrcPosition(std::stoul(ptr_rid)) - 1;
        if (position != 0) {
          head->setSSRC(std::stoul(ptr_rid));
        }
      }
      ext_buffer = ext_buffer + ext_length + 2;
      current_place = current_place + ext_length + 2;
    }
  }
  //parseLayerInfoFromVP8(packet);
  packet->compatible_temporal_layers = {0};
  packet->compatible_spatial_layers = {position};
  ELOG_WARN("SL %d TL %d", position, 0);
}

void LayerDetectorHandler::parseLayerInfoFromVP8(std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadVP8* payload = vp8_parser_.parseVP8(
      start_buffer, packet->length - rtp_header->getHeaderLength());
  if (payload->hasPictureID) {
    packet->picture_id = payload->pictureID;
  }
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

int16_t LayerDetectorHandler::getStreamIdExtensionIndex(std::array<RTPExtensions, 10> map) {
  RTPExtensions *stream_id = std::find(std::begin(map), std::end(map), RTPExtensions::RTP_ID);

  return (stream_id == std::end(map)) ? -1 : std::distance(std::begin(map), stream_id);
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

  rid_index_ = getStreamIdExtensionIndex(connection_->getRtpExtensionProcessor().getVideoExtensionMap());

  video_ssrc_list_ = connection_->getVideoSourceSSRCList();
}
}  // namespace erizo
