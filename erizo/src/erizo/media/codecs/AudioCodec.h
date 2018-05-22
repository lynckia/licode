/**
* AudioCodec.h
*/
#ifndef ERIZO_SRC_ERIZO_MEDIA_CODECS_AUDIOCODEC_H_
#define ERIZO_SRC_ERIZO_MEDIA_CODECS_AUDIOCODEC_H_

#define __STDC_CONSTANT_MACROS
extern "C" {
  #include <libavutil/avutil.h>
  #include <libavcodec/avcodec.h>
}

#include "media/MediaInfo.h"
#include "./Codecs.h"
#include "./Coder.h"
#include "./logger.h"

namespace erizo {

typedef std::function<void(AVPacket *av_packet, bool got_packet)> EncodeAudioBufferCB;

class AudioEncoder {
  DECLARE_LOGGER();

 public:
  AudioEncoder();
  virtual ~AudioEncoder();
  int initEncoder(const AudioCodecInfo& info);
  void encodeAudioBuffer(unsigned char* inBuffer, int nSamples, AVPacket* pkt,
      EncodeAudioBufferCB &done);
  int closeEncoder();
  bool initialized;

 private:
  Coder coder_;
  AVCodec *encode_codec_;
  AVCodecContext *encode_context_;
  AVFrame *encoded_frame_;
};

class AudioDecoder {
  DECLARE_LOGGER();

 public:
  AudioDecoder();
  virtual ~AudioDecoder();
  int initDecoder(const AudioCodecInfo& info);
  int decodeAudio(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff, int outBuffLen);
  int closeDecoder();
  bool initialized;

 private:
  Coder coder_;
  AVCodec* decode_codec_;
  AVCodecContext* decode_context_;
  AVFrame* dFrame_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_CODECS_AUDIOCODEC_H_
