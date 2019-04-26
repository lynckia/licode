/*
 * RtpExtensionProcessor.cpp
 */
#include "rtp/RtpExtensionProcessor.h"
#include <map>
#include <string>
#include <vector>
#include <utility>

#include "lib/Clock.h"

namespace erizo {
DEFINE_LOGGER(RtpExtensionProcessor, "rtp.RtpExtensionProcessor");

RtpExtensionProcessor::RtpExtensionProcessor(std::vector<ExtMap> ext_mappings) :
    ext_mappings_{std::move(ext_mappings)}, translationMap_{RtpExtensionProcessor::createTranslationMap()},
    video_orientation_{kVideoRotation_0} {
}

void RtpExtensionProcessor::setSdpInfo(const SdpInfo& theInfo) {
  // We build the Extension Map
  for (const auto &extension : theInfo.extMapVector) {
    if (extension.mediaType == VIDEO_TYPE || extension.mediaType == AUDIO_TYPE) {
      if (isValidExtension(extension.uri)) {
        ELOG_DEBUG("Adding RTP Extension for %s %s, value %u",
                   extension.mediaType == AUDIO_TYPE ? "Audio" : "Video",
                   extension.uri.c_str(),
                   extension.value);
        auto item = translationMap_.find(extension.uri);
        if (item != translationMap_.end()) {
          auto& translator =
              extension.mediaType == AUDIO_TYPE ? audio_extension_id_translator_ : video_extension_id_translator_;
          translator.addDestMapItem(std::make_pair(extension.value, item->second));
        }
      } else {
        ELOG_WARN("Unsupported extension %s", extension.uri.c_str());
      }
    } else {
      ELOG_WARN("Unknown type of extMap, ignoring");
    }
  }
}

bool RtpExtensionProcessor::isValidExtension(std::string uri) const {
  auto value = std::find_if(ext_mappings_.begin(), ext_mappings_.end(), [uri](const ExtMap &extension) {
    return uri == extension.uri;
  });
  return value != ext_mappings_.end() && translationMap_.find(uri) != translationMap_.end();
}

uint32_t RtpExtensionProcessor::processRtpExtensions(std::shared_ptr<DataPacket> p) {
  RtpHeader* head = reinterpret_cast<RtpHeader*>(p->data);
  uint32_t len = p->length;

  if ((p->type == VIDEO_PACKET || p->type == AUDIO_PACKET) &&
      head->getExtension() && head->getExtId() == 0xBEDE) {
    uint16_t totalExtLength = head->getExtLength();
    char* extBuffer = reinterpret_cast<char*>(&head->extensions);
    uint8_t extByte = 0;
    uint16_t currentPlace = 1;
    uint8_t extId = 0;
    uint8_t extLength = 0;
    while (currentPlace < (totalExtLength * 4)) {
      extByte = (uint8_t) (*extBuffer);
      if (extByte != RtpExtentionIdTranslator::unknown) {
        extId = extByte >> 4;
        if (extId == 15) {  // https://tools.ietf.org/html/rfc5285#section-4.2
          return len;
        }
        extLength = extByte & 0x0F;
        const auto& translator =
            p->type == VIDEO_PACKET ? video_extension_id_translator_ : audio_extension_id_translator_;
        switch (translator.getSrcExtention(extId)) {
          case ABS_SEND_TIME:
            processAbsSendTime(extBuffer);
            break;
          case VIDEO_ORIENTATION:
            processVideoOrientation(extBuffer);
            break;
          default:break;
        }
        translateExtension(extBuffer, translator);
        extBuffer += extLength + 2;
        currentPlace += extLength + 2;
      } else {  // It's padding (https://tools.ietf.org/html/rfc5285#section-4.2), skip this byte
        extBuffer++;
        currentPlace++;
      }
    }
  }
  return len;
}

uint32_t RtpExtensionProcessor::processVideoOrientation(char* buf) {
  auto* head = reinterpret_cast<const VideoOrientation*>(buf);
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

void RtpExtensionProcessor::translateExtension(void* extBuffer, const RtpExtentionIdTranslator& translator) {
  auto* data = reinterpret_cast<char*>(extBuffer);
  uint8_t src_id = data[0] >> 4;
  uint8_t len = data[0] & (uint8_t) 0x0f;
  auto dest_id = (uint8_t) translator.translateId(src_id);
  if (dest_id != RtpExtentionIdTranslator::unknown) {
    if (dest_id != src_id) {
      ELOG_DEBUG("Changing extension id from %d to %d", (int) src_id, (int) dest_id);
    }
    data[0] = (dest_id << 4) | len;
  } else {
    // clear extension
    ELOG_DEBUG("Erasing extension with id %d", (int) src_id);
    for (uint8_t i = 0; i < len + 2; ++i) {
      data[i] = RtpExtentionIdTranslator::padding;
    }
  }
}

uint32_t RtpExtensionProcessor::stripExtension(char* buf, int len) {
  // TODO(pedro)
  return len;
}

std::map<std::string, RTPExtensions> RtpExtensionProcessor::createTranslationMap() {
  return std::map<std::string, RTPExtensions>{
      {"urn:ietf:params:rtp-hdrext:ssrc-audio-level", SSRC_AUDIO_LEVEL},
      {"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time", ABS_SEND_TIME},
      {"urn:ietf:params:rtp-hdrext:toffset", TOFFSET},
      {"urn:3gpp:video-orientation", VIDEO_ORIENTATION},
      {"http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01", TRANSPORT_CC},
      {"http://www.webrtc.org/experiments/rtp-hdrext/playout-delay", PLAYBACK_TIME},
      {"urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id", RTP_ID }
  };
}
}  // namespace erizo
