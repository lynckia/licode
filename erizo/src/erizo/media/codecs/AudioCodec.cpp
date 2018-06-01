/**
 * AudioCodec.pp
 */
#include "media/codecs/AudioCodec.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(AudioEncoder, "media.codecs.AudioEncoder");
DEFINE_LOGGER(AudioDecoder, "media.codecs.AudioDecoder");

inline AVCodecID AudioCodecID2ffmpegDecoderID(AudioCodecID codec) {
  switch (codec) {
    case AUDIO_CODEC_PCM_U8: return AV_CODEC_ID_PCM_U8;
    case AUDIO_CODEC_VORBIS: return AV_CODEC_ID_VORBIS;
    default: return AV_CODEC_ID_PCM_U8;
  }
}

AudioEncoder::AudioEncoder() {
  resampled_frame_ = av_frame_alloc();
  if (!resampled_frame_)
    ELOG_ERROR("Could not allocate a frame for resampling.");
}

AudioEncoder::~AudioEncoder() {
  ELOG_DEBUG("AudioEncoder Destructor");
  this->closeCodec();
}

int AudioEncoder::initEncoder(const AudioCodecInfo& mediaInfo) {
  const InitContextBeforeOpenCB callback = [mediaInfo](AVCodecContext *context, AVDictionary *dict) {
    context->sample_fmt = AV_SAMPLE_FMT_FLT;
    // codec_context_->bit_rate = mediaInfo.bitRate;
    context->sample_rate = 8 /*mediaInfo.sampleRate*/;
    context->channels = 1;
  };

  AVCodecID av_codec_id = AudioCodecID2ffmpegDecoderID(mediaInfo.codec);
  return this->initEncoder(av_codec_id, callback);
}

int AudioEncoder::closeCodec() {
  CoderEncoder::closeCodec();
  if (resampled_frame_ != nullptr) {
    av_frame_free(&resampled_frame_);
  }
  if (audio_fifo_ != nullptr) {
    av_audio_fifo_free(audio_fifo_);
  }
  return 0;
}

void AudioEncoder::encodeAudioBuffer(unsigned char* inBuffer, int nSamples, AVPacket* pkt,
    const EncodeAudioBufferCB &done) {
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    ELOG_ERROR("Could not allocate audio frame for encoding.");
    return;
  }
  int ret;

  frame->nb_samples = codec_context_->frame_size;

  if ((ret = avcodec_fill_audio_frame(frame,
                  codec_context_->channels,
                  codec_context_->sample_fmt,
                  (const uint8_t*)inBuffer,
                  nSamples * 2,
                  0)) < 0) {
    ELOG_ERROR("avcodec_fill_audio_frame failed: %s", av_err2str_cpp(ret));
    return;
  }

  EncodeCB done_callback = [done](AVPacket *pkt, AVFrame *frame, bool got_packet) {
    av_frame_free(&frame);
    done(pkt, got_packet);
  };
  this->encode(frame, pkt, done_callback);
}

bool AudioEncoder::resampleAudioFrame(AVFrame *frame, AVFrame **o_frame) {
  uint8_t **converted_input_samples = NULL;
  int ret;
  int out_linesize;
  bool got_output = false;
  AVFrame *out_frame = resampled_frame_;

  if (this->initConvertedSamples(&converted_input_samples, frame->nb_samples, &out_linesize)) {
    if ((ret = avresample_convert(avr_context_,
                                    converted_input_samples,
                                    out_linesize,
                                    codec_context_->frame_size,
                                    frame->extended_data,
                                    frame->linesize[0],
                                    frame->nb_samples)) < 0) {
      ELOG_ERROR("Could not convert input samples (error '%s')", av_err2str_cpp(ret));
    } else if (ret == 0) {
      ELOG_WARN("0 Audio samples converted.");
    } else if (ret > 0) {
      ELOG_DEBUG("%d Audio samples converted.", ret);
      this->addSamplesToFifo(converted_input_samples, ret);
      if (av_audio_fifo_size(audio_fifo_) >= codec_context_->frame_size) {
        out_frame->nb_samples      = codec_context_->frame_size;
        out_frame->channel_layout  = codec_context_->channel_layout;
        out_frame->format          = codec_context_->sample_fmt;
        out_frame->sample_rate     = codec_context_->sample_rate;
        if (av_frame_get_buffer(out_frame, 0) < 0) {
          ELOG_ERROR("Could not allocate output frame samples");
        } else {
          if (av_audio_fifo_read(audio_fifo_, reinterpret_cast<void **>(out_frame->data),
                codec_context_->frame_size) != codec_context_->frame_size) {
            ELOG_ERROR("Could not read data from audio FIFO.");
          } else {
            if (av_frame_make_writable(out_frame) != 0) {
              ELOG_ERROR("Audio frame not writeable!");
            } else {
              got_output = true;
            }
          }
        }
      }
    }
  }
  *o_frame = out_frame;
  return got_output;
}

bool AudioEncoder::initAudioResampler(AVCodecContext *decode_ctx) {
  avr_context_ = avresample_alloc_context();
  if (!avr_context_) {
    ELOG_ERROR("Could not allocate resampler context");
    return false;
  }
  av_opt_set_int(avr_context_, "in_channel_layout",     decode_ctx->channel_layout, 0);
  av_opt_set_int(avr_context_, "in_sample_rate",        decode_ctx->sample_rate, 0);
  av_opt_set_int(avr_context_, "in_sample_fmt",         decode_ctx->sample_fmt, 0);
  av_opt_set_int(avr_context_, "out_channel_layout",    codec_context_->channel_layout, 0);
  av_opt_set_int(avr_context_, "out_sample_rate",       codec_context_->sample_rate, 0);
  av_opt_set_int(avr_context_, "out_sample_fmt",        codec_context_->sample_fmt, 0);

  if (avresample_open(avr_context_) < 0) {
    ELOG_ERROR("Failed to initialize resampling context");
    return false;
  }

  if (!(audio_fifo_ = av_audio_fifo_alloc(codec_context_->sample_fmt,
                                           codec_context_->channels, 1))) {
    ELOG_ERROR("Could not allocate audio fifo");
    return false;
  }
  ELOG_DEBUG("Initialized Audio resample context.");
  return true;
}

bool AudioEncoder::initConvertedSamples(uint8_t ***converted_input_samples, int frame_size, int *out_linesize) {
    int ret;
    static char error_str[255];

    if (!(*converted_input_samples = reinterpret_cast<uint8_t **>(calloc(codec_context_->channels,
                                            sizeof(**converted_input_samples))))) {
      ELOG_ERROR("Could not allocate converted input sample pointers");
      return false;
    }

    if ((ret = av_samples_alloc(*converted_input_samples, out_linesize,
                                  codec_context_->channels,
                                  frame_size,
                                  codec_context_->sample_fmt, 0)) < 0) {
      ELOG_ERROR("Could not allocate converted input samples: %s",
          av_strerror(ret, error_str, sizeof(error_str)));
      av_freep(&(*converted_input_samples)[0]);
      free(*converted_input_samples);
      return false;
    }
    return true;
}

bool AudioEncoder::addSamplesToFifo(uint8_t **converted_input_samples, const int frame_size) {
  if (av_audio_fifo_realloc(audio_fifo_, av_audio_fifo_size(audio_fifo_) + frame_size) < 0) {
      ELOG_ERROR("Could not reallocate FIFO.");
      return false;
  }
  if (av_audio_fifo_write(audio_fifo_, reinterpret_cast<void **>(converted_input_samples),
                          frame_size) < frame_size) {
      ELOG_ERROR("Could not write data to FIFO");
      return false;
  }
  return true;
}

int AudioEncoder::getBestSampleRate(const AVCodec *codec) {
  const int *p;
  int best_samplerate = 44100;
  if (!codec->supported_samplerates) {
    return best_samplerate;
  }
  p = codec->supported_samplerates;
  while (*p) {
    best_samplerate = FFMAX(*p, best_samplerate);
    p++;
  }
  return best_samplerate;
}

uint64_t AudioEncoder::getChannelLayout(const AVCodec *codec) {
  const uint64_t *p;
  uint64_t best_ch_layout = 0;
  uint64_t best_nb_channels = 0;
  if (!codec->channel_layouts)
    return AV_CH_LAYOUT_STEREO;
  p = codec->channel_layouts;
  while (*p) {
    uint64_t nb_channels = av_get_channel_layout_nb_channels(*p);
    if (nb_channels > best_nb_channels) {
      best_ch_layout = *p;
      best_nb_channels = nb_channels;
    }
    p++;
  }
  return best_ch_layout;
}

int AudioDecoder::initDecoder(const AudioCodecInfo& info) {
  const InitContextBeforeOpenCB callback = [info](AVCodecContext *context, AVDictionary *dict) {
    context->sample_fmt = AV_SAMPLE_FMT_S16;
    context->bit_rate = info.bitRate;
    context->sample_rate = info.sampleRate;
    context->channels = 1;
  };

  AVCodecID av_codec_id = AudioCodecID2ffmpegDecoderID(info.codec);
  return this->initDecoder(av_codec_id, callback);
}

int AudioDecoder::decodeAudio(unsigned char* inBuff, int inBuffLen,
    unsigned char* outBuff, int outBuffLen) {

  int out_size = 0;
  AVPacket avpkt;
  av_init_packet(&avpkt);
  avpkt.data = (unsigned char*) inBuff;
  avpkt.size = inBuffLen;

  AVFrame *frame;
  frame = av_frame_alloc();

  if (this->decode(frame, &avpkt)) {
    // Asume S16 Non-Planar Audio for now.
    int planar = av_sample_fmt_is_planar(codec_context_->sample_fmt);
    if (planar == 0) {
      out_size = av_samples_get_buffer_size(NULL,
                          av_frame_get_channels(frame),
                          frame->nb_samples,
                          (AVSampleFormat)frame->format, 1);
      if (out_size > outBuffLen) {
        ELOG_ERROR("Data size bigger than buffer size.");
        out_size = 0;
      } else {
        memcpy(outBuff, frame->data[0], out_size);
      }
    } else {
      ELOG_ERROR("Audio planar buffer is not handled yet.");
    }
    av_packet_unref(&avpkt);
  }

  av_frame_free(&frame);
  return out_size;
}
}  // namespace erizo
