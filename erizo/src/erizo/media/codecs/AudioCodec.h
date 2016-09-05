/**
 * VideoCodec.h
 */

#ifndef AUDIOCODEC_H_
#define AUDIOCODEC_H_

#include "Codecs.h"
#include "logger.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include "libavutil/audio_fifo.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/frame.h"
#include "libavutil/opt.h"
#include <libavformat/avformat.h>

}

namespace erizo {

  class AudioEncoder {
    DECLARE_LOGGER();
    public:
      AudioEncoder();
      virtual ~AudioEncoder();
      int encodeAudio(unsigned char* inBuffer, int nSamples, AVPacket* pkt);
      int closeEncoder();

    private:
      AVCodec* codec_;
      AVCodecContext* codecContext_;
  };

  class AudioDecoder {
    DECLARE_LOGGER();
    public:
      AudioDecoder();
      virtual ~AudioDecoder();
      int initDecoder(AVCodecContext* context, AVCodec* dec_codec);
      int decodeAudio(AVPacket& input_packet, AVPacket& output_packet);
      int closeDecoder();

      ////////////////added func so logger inside available//////////////////
      int init_resampler(AVCodecContext *input_codec_context, AVCodecContext *output_codec_context);
      int add_samples_to_fifo(AVAudioFifo *fifo, uint8_t **converted_input_samples, const int frame_size);

      int init_fifo(AVAudioFifo **fifo);
      int init_output_frame(AVFrame **frame, AVCodecContext *output_codec_context, int frame_size);
      int encode_audio_frame(AVFrame *frame, AVPacket& output_packet);
      int load_encode(AVPacket& output_packet);
      int init_converted_samples(uint8_t ***converted_input_samples, AVCodecContext *output_codec_context, int frame_size);
      int convert_samples(const uint8_t **input_data, uint8_t **converted_data, const int frame_size, SwrContext *resample_context);
     ////////////// 

    private:
      AVCodec* codec_;
  };
}
#endif /* AUDIOCODEC_H_ */
