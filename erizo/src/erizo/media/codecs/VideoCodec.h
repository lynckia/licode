/**
 * VideoCodec.h
 */

#ifndef ERIZO_SRC_ERIZO_MEDIA_CODECS_VIDEOCODEC_H_
#define ERIZO_SRC_ERIZO_MEDIA_CODECS_VIDEOCODEC_H_

#include "media/codecs/Codecs.h"
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

class VideoEncoder {
  DECLARE_LOGGER();

 public:
  VideoEncoder();
  virtual ~VideoEncoder();
  int initEncoder(const VideoCodecInfo& info);
  int encodeVideo(unsigned char* inBuffer, int length, unsigned char* outBuffer, int outLength);
  int closeEncoder();
  void requestKeyframe();
  void setTargetBitrate(uint64_t bitrate);
  void setResolution(int width, int height);

 private:
  void restartContext();
  void maybeSwapContext();

 private:
  AVCodec* vCoder;
  AVCodecContext* coder_context_, *next_coder_context_;
  AVFrame* cPicture;
  bool keyframe_requested_;
  uint64_t target_bitrate_;
  int target_width_;
  int target_height_;
};

class VideoDecoder {
  DECLARE_LOGGER();

 public:
  VideoDecoder();
  virtual ~VideoDecoder();
  int initDecoder(const VideoCodecInfo& info);
  int initDecoder(AVCodecContext* context);
  int decodeVideo(unsigned char* inBuff, int inBuffLen,
      unsigned char* outBuff, int outBuffLen, int* gotFrame);
  int closeDecoder();

 private:
  AVCodec* vDecoder;
  bool initWithContext_;
  AVCodecContext* vDecoderContext;
  AVFrame* dPicture;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_CODECS_VIDEOCODEC_H_
