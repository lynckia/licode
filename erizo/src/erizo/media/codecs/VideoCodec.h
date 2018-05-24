/**
 * VideoCodec.h
 */

#ifndef ERIZO_SRC_ERIZO_MEDIA_CODECS_VIDEOCODEC_H_
#define ERIZO_SRC_ERIZO_MEDIA_CODECS_VIDEOCODEC_H_

#include <functional>

#include "media/MediaInfo.h"
#include "media/codecs/Codecs.h"
#include "media/codecs/Coder.h"
#include "./logger.h"

extern "C" {
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}

namespace erizo {

typedef std::function<void(bool success, int len)> EncodeVideoBufferCB;

class VideoEncoder : public CoderEncoder {
  DECLARE_LOGGER();

 public:
  using CoderEncoder::initEncoder;
  int initEncoder(const VideoCodecInfo& info);
  void encodeVideoBuffer(unsigned char* inBuffer, int len, unsigned char* outBuffer,
      const EncodeVideoBufferCB &done);
};

class VideoDecoder : public CoderDecoder {
  DECLARE_LOGGER();

 public:
  using CoderDecoder::initDecoder;
  int initDecoder(const VideoCodecInfo& info);
  int initDecoder(AVCodecParameters *codecpar);
  bool decode(AVFrame *frame, AVPacket *av_packet);
  int decodeVideoBuffer(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff,
      int outBuffLen, int* gotFrame);
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_CODECS_VIDEOCODEC_H_
