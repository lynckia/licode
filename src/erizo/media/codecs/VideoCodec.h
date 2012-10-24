/**
 * VideoCodec.h
 */

#ifndef VIDEOCODEC_H_
#define VIDEOCODEC_H_

#include "Codecs.h"
//Forward Declarations

struct AVCodec;
struct AVCodecContext;
struct AVFrame;

namespace erizo {

  class VideoEncoder {
    public:
      VideoEncoder();
      int initEncoder (const VideoCodecInfo& info);
      int encodeVideo (uint8_t* inBuffer, int length, 
          uint8_t* outBuffer, int outLength, bool& hasFrame);
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
      int decodeVideo(uint8_t* inBuff, int inBuffLen,
          uint8_t* outBuff, int outBuffLen, bool* gotFrame);
      int closeDecoder();

    private:
      AVCodec* vDecoder;
      AVCodecContext* vDecoderContext;
      AVFrame* dPicture;
  };

}
#endif /* VIDEOCODEC_H_ */
