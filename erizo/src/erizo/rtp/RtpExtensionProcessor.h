#ifndef ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_

#include <array>
#include <map>
#include <string>
#include <vector>

#include "MediaDefinitions.h"
#include "SdpInfo.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RTPExtensions.h"
#include "rtp/RtpExtentionIdTranslator.h"

namespace erizo {

class RtpExtensionProcessor {
  DECLARE_LOGGER();

 public:
  explicit RtpExtensionProcessor(std::vector<erizo::ExtMap> ext_mappings);
  virtual ~RtpExtensionProcessor() = default;

  void setSdpInfo(const SdpInfo& theInfo);
  uint32_t processRtpExtensions(std::shared_ptr<DataPacket> p);
  VideoRotation getVideoRotation() const { return video_orientation_; }

  const RTPExtensionsMap& getVideoExtensionMap() const {
    return video_extension_id_translator_.getDestMap();
  }
  const RTPExtensionsMap& getAudioExtensionMap() const {
    return audio_extension_id_translator_.getDestMap();
  }
  std::vector<ExtMap> getSupportedExtensionMap() {
    return ext_mappings_;
  }
  void setSourceExtensionMap(const std::pair<RTPExtensionsMap, RTPExtensionsMap>& ext_map_pair) {
    video_extension_id_translator_.setSourceMap(ext_map_pair.first);
    audio_extension_id_translator_.setSourceMap(ext_map_pair.second);
  }
  bool isValidExtension(std::string uri) const;

 private:
  std::vector<ExtMap> ext_mappings_;
  RtpExtentionIdTranslator audio_extension_id_translator_;
  RtpExtentionIdTranslator video_extension_id_translator_;
  const std::map<std::string, RTPExtensions> translationMap_;
  VideoRotation video_orientation_;
  uint32_t processAbsSendTime(char* buf);
  uint32_t processVideoOrientation(char* buf);
  uint32_t stripExtension(char* buf, int len);
  static std::map<std::string, RTPExtensions> createTranslationMap();
  static void translateExtension(void* extBuffer, const RtpExtentionIdTranslator& translator);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_
