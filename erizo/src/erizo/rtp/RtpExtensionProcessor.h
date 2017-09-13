#ifndef ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_

#include <array>
#include <map>
#include <string>
#include <vector>

#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "rtp/RtpHeaders.h"

namespace erizo {

enum RTPExtensions {
  UNKNOWN = 0,
  SSRC_AUDIO_LEVEL,     // urn:ietf:params:rtp-hdrext:ssrc-audio-level
  ABS_SEND_TIME,        // http:// www.webrtc.org/experiments/rtp-hdrext/abs-send-time
  TOFFSET,              // urn:ietf:params:rtp-hdrext:toffset
  VIDEO_ORIENTATION,    // urn:3gpp:video-orientation
  TRANSPORT_CC,         // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
  PLAYBACK_TIME,        // http:// www.webrtc.org/experiments/rtp-hdrext/playout-delay
  RTP_ID                // urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
};

class RtpExtensionProcessor{
  DECLARE_LOGGER();

 public:
  explicit RtpExtensionProcessor(const std::vector<erizo::ExtMap> ext_mappings);
  virtual ~RtpExtensionProcessor();

  void setSdpInfo(const SdpInfo& theInfo);
  uint32_t processRtpExtensions(std::shared_ptr<DataPacket> p);

  std::array<RTPExtensions, 10> getVideoExtensionMap() {
    return ext_map_video_;
  }
  std::array<RTPExtensions, 10> getAudioExtensionMap() {
    return ext_map_audio_;
  }
  std::vector<ExtMap> getSupportedExtensionMap() {
    return ext_mappings_;
  }
  bool isValidExtension(std::string uri);

 private:
  std::vector<ExtMap> ext_mappings_;
  std::array<RTPExtensions, 10> ext_map_video_, ext_map_audio_;
  std::map<std::string, uint8_t> translationMap_;
  uint32_t processAbsSendTime(char* buf);
  uint32_t stripExtension(char* buf, int len);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_
