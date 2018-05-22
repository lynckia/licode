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
// Forward Declarations

// struct AVCodec;
// struct AVCodecContext;
// struct AVFrame;

namespace erizo {

typedef std::function<void(bool success, int len)> EncodeVideoBufferCB;

class VideoEncoder {
  DECLARE_LOGGER();

 public:
  VideoEncoder();
  virtual ~VideoEncoder();
  int initEncoder(const VideoCodecInfo& info);
  void encodeVideoBuffer(unsigned char* inBuffer, int len, unsigned char* outBuffer,
      const EncodeVideoBufferCB &done);
  int closeEncoder();
  bool initialized;

 private:
  Coder coder_;
  AVCodec* av_codec;
  AVCodecContext* encode_context_;
  AVFrame* cPicture;
};

class VideoDecoder {
  DECLARE_LOGGER();

 public:
  VideoDecoder();
  virtual ~VideoDecoder();
  int initDecoder(const VideoCodecInfo& info);
  int initDecoder(AVCodecContext** context, AVCodecParameters *codecpar);
  int decodeVideoBuffer(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff,
      int outBuffLen, int* gotFrame);
  int closeDecoder();
  bool initialized;

 private:
  Coder coder_;
  AVCodec* av_codec;
  AVCodecContext* decode_context_;
  AVFrame* dPicture;
  bool initWithContext_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_CODECS_VIDEOCODEC_H_
