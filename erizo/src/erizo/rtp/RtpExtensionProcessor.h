#ifndef RTPEXTENSIONPROCESSOR_H_
#define RTPEXTENSIONPROCESSOR_H_

#include <vector>

#include "MediaDefinitions.h"
#include "SdpInfo.h"
#include "rtp/RtpHeaders.h"

namespace erizo {

  enum RTPExtensions {
    SSRC_AUDIO_LEVEL,     // urn:ietf:params:rtp-hdrext:ssrc-audio-level
    ABS_SEND_TIME,        // http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
    TOFFSET,              // urn:ietf:params:rtp-hdrext:toffset
    VIDEO_ORIENTATION     // urn:3gpp:video-orientation 
  };

  class RtpExtensionProcessor{
    DECLARE_LOGGER();

    public:
    RtpExtensionProcessor();
    virtual ~RtpExtensionProcessor();

    void setSdpInfo (const SdpInfo& theInfo);
    uint32_t processRtpExtensions(dataPacket& p);

    private:
    int extMapVideo_[10], extMapAudio_[10];
    std::map<std::string, uint8_t> translationMap_;
    uint32_t processAbsSendTime(char* buf);
    uint32_t stripExtension (char* buf, int len);

  };

} /* namespace erizo */

#endif /* RTPEXTENSIONPROCESSOR_H_ */
