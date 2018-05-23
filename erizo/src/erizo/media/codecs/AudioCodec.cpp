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
  encode_codec_ = NULL;
  encode_context_ = NULL;
  encoded_frame_ = NULL;
  initialized = false;
}

AudioEncoder::~AudioEncoder() {
  ELOG_DEBUG("AudioEncoder Destructor");
  this->closeEncoder();
}

int AudioEncoder::initEncoder(const AudioCodecInfo& mediaInfo) {
  if (!coder_.allocCodecContext(&encode_context_, &encode_codec_,
      AudioCodecID2ffmpegDecoderID(mediaInfo.codec), OPERATION_ENCODE)) {
    return -1;
  }

  encode_context_->sample_fmt = AV_SAMPLE_FMT_FLT;
  // encode_context_->bit_rate = mediaInfo.bitRate;
  encode_context_->sample_rate = 8 /*mediaInfo.sampleRate*/;
  encode_context_->channels = 1;

  initialized = coder_.openCodecContext(encode_context_, encode_codec_, NULL);
  return initialized;
}

int AudioEncoder::closeEncoder() {
  if (encode_context_ != nullptr) {
    avcodec_close(encode_context_);
    avcodec_free_context(&encode_context_);
  }
  if (encoded_frame_ != nullptr) {
    av_frame_free(&encoded_frame_);
  }
  return 0;
}

void AudioEncoder::encodeAudioBuffer(unsigned char* inBuffer, int nSamples, AVPacket* pkt,
    EncodeAudioBufferCB &done) {
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    ELOG_ERROR("Could not allocate audio frame for encoding.");
    return;
  }
  int ret;

  frame->nb_samples = encode_context_->frame_size;

  if ((ret = avcodec_fill_audio_frame(frame,
                  encode_context_->channels,
                  encode_context_->sample_fmt,
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
  coder_.encode(encode_context_, frame, pkt, done_callback);
}

AudioDecoder::AudioDecoder() {
  decode_codec_ = NULL;
  decode_context_ = NULL;
  dFrame_ = NULL;
  initialized = false;
}

AudioDecoder::~AudioDecoder() {
  ELOG_DEBUG("AudioDecoder Destructor");
  this->closeDecoder();
}

int AudioDecoder::initDecoder(const AudioCodecInfo& info) {
  if (!coder_.allocCodecContext(&decode_context_, &decode_codec_,
      AudioCodecID2ffmpegDecoderID(info.codec), OPERATION_DECODE)) {
    return -1;
  }

  decode_context_->sample_fmt = AV_SAMPLE_FMT_S16;
  decode_context_->bit_rate = info.bitRate;
  decode_context_->sample_rate = info.sampleRate;
  decode_context_->channels = 1;

  initialized = coder_.openCodecContext(decode_context_, decode_codec_, NULL);
  return initialized;
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

  if (coder_.decode(decode_context_, frame, &avpkt)) {
    // Asume S16 Non-Planar Audio for now.
    int planar = av_sample_fmt_is_planar(decode_context_->sample_fmt);
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

int AudioDecoder::closeDecoder() {
  if (decode_context_ != NULL) {
    avcodec_close(decode_context_);
  }
  if (dFrame_ != NULL) {
    av_frame_free(&dFrame_);
  }
  initialized = false;
  return 0;
}
}  // namespace erizo
