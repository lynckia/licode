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

class AudioEncoder : public CoderEncoder {
  DECLARE_LOGGER();

 public:
  AudioEncoder();
  virtual ~AudioEncoder();
  using CoderEncoder::initEncoder;
  int initEncoder(const AudioCodecInfo& info);
  int closeCodec() override;
  bool initAudioResampler(AVCodecContext *decode_ctx);
  void encodeAudioBuffer(unsigned char* inBuffer, int nSamples, AVPacket* pkt,
      const EncodeAudioBufferCB &done);
  bool resampleAudioFrame(AVFrame *frame, AVFrame **out_frame);
  bool initConvertedSamples(uint8_t ***converted_input_samples, int frame_size, int *out_linesize);
  bool addSamplesToFifo(uint8_t **converted_input_samples, const int frame_size);
  uint64_t getChannelLayout(const AVCodec *codec);
  int getBestSampleRate(const AVCodec *codec);

 public:
  AVFrame *resampled_frame_;

 private:
  AVAudioFifo *audio_fifo_;
  AVAudioResampleContext *avr_context_;
};

class AudioDecoder : public CoderDecoder {
  DECLARE_LOGGER();

 public:
  using CoderDecoder::initDecoder;
  int initDecoder(const AudioCodecInfo& info);
  int decodeAudio(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff, int outBuffLen);
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_CODECS_AUDIOCODEC_H_
