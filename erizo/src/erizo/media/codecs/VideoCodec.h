/**
 * VideoCodec.h
 */

#ifndef VIDEOCODEC_H_
#define VIDEOCODEC_H_

#include "Codecs.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}
//Forward Declarations

struct AVCodec;
struct AVCodecContext;
struct AVFrame;

namespace erizo {

  class VideoEncoder {
    public:
      VideoEncoder();
      int initEncoder (const VideoCodecInfo& info);
      int encodeVideo (unsigned char* inBuffer, int length, 
          unsigned char* outBuffer, int outLength, int& hasFrame);
      int closeEncoder ();

    private:
      AVCodec* vCoder;
      AVCodecContext* vCoderContext;
      AVFrame* cPicture;
  };

  class VideoDecoder {
    public:
      VideoDecoder();
      int initDecoder (const VideoCodecInfo& info);
      int decodeVideo(unsigned char* inBuff, int inBuffLen,
          unsigned char* outBuff, int outBuffLen, int* gotFrame);
      int closeDecoder();

    private:
      AVCodec* vDecoder;
      AVCodecContext* vDecoderContext;
      AVFrame* dPicture;
  };

}
#endif /* VIDEOCODEC_H_ */
