#ifndef ERIZO_SRC_ERIZO_MEDIA_MEDIAINFO_H_
#define ERIZO_SRC_ERIZO_MEDIA_MEDIAINFO_H_

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <string>
#include "codecs/Codecs.h"

namespace erizo {

struct RTPInfo {
  enum AVCodecID codec;
  unsigned int ssrc;
  unsigned int PT;
};

enum ProcessorType {
  RTP_ONLY, AVF, PACKAGE_ONLY
};

struct MediaInfo {
  std::string url;
  bool hasVideo;
  bool hasAudio;
  ProcessorType processorType;
  RTPInfo rtpVideoInfo;
  RTPInfo rtpAudioInfo;
  VideoCodecInfo videoCodec;
  AudioCodecInfo audioCodec;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_MEDIA_MEDIAINFO_H_
