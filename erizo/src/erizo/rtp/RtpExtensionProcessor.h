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
  PLAYBACK_TIME         //  http:// www.webrtc.org/experiments/rtp-hdrext/playout-delay
};

class RtpExtensionProcessor{
  DECLARE_LOGGER();

 public:
  RtpExtensionProcessor();
  virtual ~RtpExtensionProcessor();

  void setSdpInfo(const SdpInfo& theInfo);
  uint32_t processRtpExtensions(std::shared_ptr<dataPacket> p);

  std::array<RTPExtensions, 10> getVideoExtensionMap() {
    return ext_map_video_;
  }
  std::array<RTPExtensions, 10> getAudioExtensionMap() {
    return ext_map_audio_;
  }

 private:
  std::array<RTPExtensions, 10> ext_map_video_, ext_map_audio_;
  std::map<std::string, uint8_t> translationMap_;
  uint32_t processAbsSendTime(char* buf);
  uint32_t stripExtension(char* buf, int len);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_
