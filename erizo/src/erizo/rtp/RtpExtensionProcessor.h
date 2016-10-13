#ifndef ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_

#include <map>
#include <string>
#include <vector>

#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "rtp/RtpHeaders.h"

namespace erizo {

enum RTPExtensions {
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
  uint32_t processRtpExtensions(const dataPacket& p);

 private:
  int extMapVideo_[10], extMapAudio_[10];
  std::map<std::string, uint8_t> translationMap_;
  uint32_t processAbsSendTime(char* buf);
  uint32_t stripExtension(char* buf, int len);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONPROCESSOR_H_
