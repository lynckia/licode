/*
 * RtpExtensionProcessor.cpp
 */
#include "rtp/RtpExtensionProcessor.h"
#include <map>
#include <string>
#include <vector>

#include "lib/Clock.h"

namespace erizo {
DEFINE_LOGGER(RtpExtensionProcessor, "rtp.RtpExtensionProcessor");

RtpExtensionProcessor::RtpExtensionProcessor(const std::vector<erizo::ExtMap> ext_mappings) :
    ext_mappings_{ext_mappings}, video_orientation_{kVideoRotation_0} {
  translationMap_["urn:ietf:params:rtp-hdrext:ssrc-audio-level"] = SSRC_AUDIO_LEVEL;
  translationMap_["http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time"] = ABS_SEND_TIME;
  translationMap_["urn:ietf:params:rtp-hdrext:toffset"] = TOFFSET;
  translationMap_["urn:3gpp:video-orientation"] = VIDEO_ORIENTATION;
  translationMap_["http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"] = TRANSPORT_CC;
  translationMap_["http://www.webrtc.org/experiments/rtp-hdrext/playout-delay"]= PLAYBACK_TIME;
  translationMap_["urn:ietf:params:rtp-hdrext:sdes:mid"] = MID;
  translationMap_["urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"]= RTP_ID;
  ext_map_video_.fill(UNKNOWN);
  ext_map_audio_.fill(UNKNOWN);
}

RtpExtensionProcessor::~RtpExtensionProcessor() {
}

void RtpExtensionProcessor::setSdpInfo(std::shared_ptr<SdpInfo> theInfo) {
  // We build the Extension Map
  for (unsigned int i = 0; i < theInfo->extMapVector.size(); i++) {
    const ExtMap& map = theInfo->extMapVector[i];
    std::map<std::string, uint8_t>::iterator it;
    if (isValidExtension(map.uri)) {
      setExtension(map.mediaType, map.value, RTPExtensions((*translationMap_.find(map.uri)).second));
    } else {
      ELOG_WARN("Unsupported extension %s", map.uri.c_str());
    }
  }
}

void RtpExtensionProcessor::setExtension(MediaType type, uint16_t internal_value, uint16_t value) {
  switch (type) {
    case VIDEO_TYPE:
      ext_map_video_[internal_value] = RTPExtensions(value);
      break;
    case AUDIO_TYPE:
      ext_map_audio_[internal_value] = RTPExtensions(value);
      break;
    default:
      ELOG_WARN("Unknown type of extMap, ignoring");
      break;
  }
}

bool RtpExtensionProcessor::isValidExtension(std::string uri) {
  auto value = std::find_if(ext_mappings_.begin(), ext_mappings_.end(), [uri](const ExtMap &extension) {
    return uri == extension.uri;
  });
  return value != ext_mappings_.end() && translationMap_.find(uri) != translationMap_.end();
}

uint32_t RtpExtensionProcessor::processRtpExtensions(std::shared_ptr<DataPacket> p) {
  const RtpHeader* head = reinterpret_cast<const RtpHeader*>(p->data);
  uint32_t len = p->length;
  std::array<RTPExtensions, 15> extMap;
  if (head->getExtension()) {
    switch (p->type) {
      case VIDEO_PACKET:
        extMap = ext_map_video_;
        break;
      case AUDIO_PACKET:
        extMap = ext_map_audio_;
        break;
      default:
        ELOG_WARN("Won't process RTP extensions for unknown type packets");
        return 0;
        break;
    }
    uint16_t total_rtp_extensions_length = head->getExtLength();
    if (head->getExtId() == 0xBEDE) {
      char* ext_buffer = (char*)&head->extensions;  // NOLINT
      uint8_t current_ext_byte = 0;
      uint16_t current_place = 1;
      uint8_t current_ext_id = 0;
      uint8_t current_ext_length = 0;
      char a[] = {'0'};
      while (current_place < (total_rtp_extensions_length*4)) {
        current_ext_byte = (uint8_t)(*ext_buffer);
        current_ext_id = current_ext_byte >> 4;
        current_ext_length = current_ext_byte & 0x0F;
        char *ext_byte;
        if (current_ext_id != 0 && extMap[current_ext_id] != 0) {
          switch (extMap[current_ext_id]) {
            case MID:
              processMid(ext_buffer);
              if (!p->mid.empty()) {
                ext_byte = ext_buffer + 1;
                ext_byte[0] = p->mid.c_str()[0];
              }
              p->mid = mid_;
              break;
            case RTP_ID:
              processRid(ext_buffer);
              ext_byte = ext_buffer + 1;
              ext_byte[0] = a[0];
              p->rid = rid_;
              break;
            case ABS_SEND_TIME:
              processAbsSendTime(ext_buffer);
              break;
            case VIDEO_ORIENTATION:
              processVideoOrientation(ext_buffer);
              break;
            default:
              break;
          }
        }
        ext_buffer = ext_buffer + current_ext_length + 2;
        current_place = current_place + current_ext_length + 2;
      }
    }
  }
  return len;
}

uint32_t RtpExtensionProcessor::removeMidAndRidExtensions(std::shared_ptr<DataPacket> p) {
  const RtpHeader* head = reinterpret_cast<const RtpHeader*>(p->data);
  uint32_t len = p->length;
  std::array<RTPExtensions, 15> extMap;
  if (head->getExtension()) {
    switch (p->type) {
      case VIDEO_PACKET:
        extMap = ext_map_video_;
        break;
      case AUDIO_PACKET:
        extMap = ext_map_audio_;
        break;
      default:
        ELOG_WARN("Won't process RTP extensions for unknown type packets");
        return 0;
        break;
    }
    uint16_t total_rtp_extensions_length = head->getExtLength();
    if (head->getExtId() == 0xBEDE) {
      char* ext_buffer = (char*)&head->extensions;  // NOLINT
      uint8_t current_ext_byte = 0;
      uint16_t current_place = 1;
      uint8_t current_ext_id = 0;
      uint8_t current_ext_length = 0;
      while (current_place < (total_rtp_extensions_length*4)) {
        current_ext_byte = (uint8_t)(*ext_buffer);
        current_ext_id = current_ext_byte >> 4;
        current_ext_length = current_ext_byte & 0x0F;
        if (current_ext_id != 0 && extMap[current_ext_id] != 0) {
          switch (extMap[current_ext_id]) {
            case MID:
            case RTP_ID:
              for (uint8_t position = 0; position <= current_ext_length + 1; position++) {
                ext_buffer[position] = 0;
              }
              break;
            case ABS_SEND_TIME:
            case VIDEO_ORIENTATION:
            default:
              break;
          }
        }
        ext_buffer = ext_buffer + current_ext_length + 2;
        current_place = current_place + current_ext_length + 2;
      }
    }
  }
  return len;
}

std::string RtpExtensionProcessor::getMid() {
  return mid_;
}

uint32_t RtpExtensionProcessor::processMid(char* buf) {
  GenericOneByteExtension* header = new GenericOneByteExtension(buf);
  mid_ = std::string(header->getData());
  delete header;
  return 0;
}

std::string RtpExtensionProcessor::getRid() {
  return rid_;
}

uint32_t RtpExtensionProcessor::processRid(char* buf) {
  GenericOneByteExtension* header = new GenericOneByteExtension(buf);
  rid_ = std::string(header->getData());
  delete header;
  return 0;
}

VideoRotation RtpExtensionProcessor::getVideoRotation() {
  return video_orientation_;
}

uint32_t RtpExtensionProcessor::processVideoOrientation(char* buf) {
  VideoOrientation* head = reinterpret_cast<VideoOrientation*>(buf);
  video_orientation_ = head->getVideoOrientation();
  return 0;
}

uint32_t RtpExtensionProcessor::processAbsSendTime(char* buf) {
  duration now = clock::now().time_since_epoch();
  AbsSendTimeExtension* head = reinterpret_cast<AbsSendTimeExtension*>(buf);
  auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now);
  auto now_usec = std::chrono::duration_cast<std::chrono::microseconds>(now);

  uint32_t now_usec_only = now_usec.count() - now_sec.count()*1e+6;

  uint8_t seconds = now_sec.count() & 0x3F;
  uint32_t absecs = now_usec_only * ((1LL << 18) - 1) * 1e-6;

  absecs = (seconds << 18) + absecs;
  head->setAbsSendTime(absecs);
  return 0;
}

uint32_t RtpExtensionProcessor::stripExtension(char* buf, int len) {
  // TODO(pedro)
  return len;
}
}  // namespace erizo
