#ifndef ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONS_H_
#define ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONS_H_

#include <ostream>

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

inline std::ostream& operator<<(std::ostream & os, const RTPExtensions& extention) {
  switch (extention) {
    case UNKNOWN:
      os << "UNKNOWN";
      break;
    case SSRC_AUDIO_LEVEL:
      os << "SSRC_AUDIO_LEVEL";
      break;
    case ABS_SEND_TIME:
      os << "ABS_SEND_TIME";
      break;
    case TOFFSET:
      os << "TOFFSET";
      break;
    case VIDEO_ORIENTATION:
      os << "VIDEO_ORIENTATION";
      break;
    case TRANSPORT_CC:
      os << "TRANSPORT_CC";
      break;
    case PLAYBACK_TIME:
      os << "PLAYBACK_TIME";
      break;
    case RTP_ID:
      os << "RTP_ID";
      break;
  }
  return os;
}

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPEXTENSIONS_H_
