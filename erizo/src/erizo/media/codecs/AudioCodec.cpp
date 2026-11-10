/**
 * AudioCodec.pp
 */
#include "media/codecs/AudioCodec.h"
#include "media/codecs/FFmpegCompat.h"

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
  aCoder_ = NULL;
  aCoderContext_ = NULL;
  aFrame_ = NULL;
}

AudioEncoder::~AudioEncoder() {
  ELOG_DEBUG("AudioEncoder Destructor");
  this->closeEncoder();
}

int AudioEncoder::initEncoder(const AudioCodecInfo& mediaInfo) {
  ELOG_DEBUG("Init audioEncoder begin");
  aCoder_ = avcodec_find_encoder(AudioCodecID2ffmpegDecoderID(mediaInfo.codec));
  if (!aCoder_) {
    ELOG_DEBUG("Audio Codec not found");
    return false;
  }

  aCoderContext_ = avcodec_alloc_context3(aCoder_);
  if (!aCoderContext_) {
    ELOG_DEBUG("Memory error allocating audio coder context");
    return false;
  }

  aCoderContext_->sample_fmt = AV_SAMPLE_FMT_FLT;
  aCoderContext_->sample_rate = 8 /*mediaInfo.sampleRate*/;
  set_codec_channels(aCoderContext_, 1);
  char errbuff[500];
  int res = avcodec_open2(aCoderContext_, aCoder_, NULL);
  if (res != 0) {
    av_strerror(res, reinterpret_cast<char*>(&errbuff), 500);
    ELOG_DEBUG("fail when opening input %s", errbuff);
    return -1;
  }
  ELOG_DEBUG("Init audioEncoder end");
  return true;
}

int AudioEncoder::encodeAudio(unsigned char* inBuffer, int nSamples, AVPacket* pkt) {
  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    ELOG_ERROR("could not allocate audio frame");
    return 0;
  }
  int ret, buffer_size;
  int channels = get_codec_channels(aCoderContext_);

  frame->nb_samples = aCoderContext_->frame_size;
  frame->format = aCoderContext_->sample_fmt;
  set_codec_channels(aCoderContext_, channels);
#if LIBAVCODEC_VERSION_MAJOR >= 59
  av_channel_layout_copy(&frame->ch_layout, &aCoderContext_->ch_layout);
#endif

  ELOG_DEBUG("channels %d, frame_size %d, sample_fmt %d",
      channels, aCoderContext_->frame_size,
      aCoderContext_->sample_fmt);
  buffer_size = av_samples_get_buffer_size(NULL, channels,
      aCoderContext_->frame_size, aCoderContext_->sample_fmt, 0);
  uint16_t* samples = reinterpret_cast<uint16_t*>(malloc(buffer_size));
  if (!samples) {
    ELOG_ERROR("could not allocate %d bytes for samples buffer", buffer_size);
    av_frame_free(&frame);
    return 0;
  }
  ret = avcodec_fill_audio_frame(frame, channels,
      aCoderContext_->sample_fmt, (const uint8_t*) samples, buffer_size,
      0);
  if (ret < 0) {
      free(samples);
      av_frame_free(&frame);
      ELOG_ERROR("could not setup audio frame");
      return 0;
  }

  ret = avcodec_send_frame(aCoderContext_, frame);
  free(samples);
  av_frame_free(&frame);
  if (ret < 0) {
    ELOG_ERROR("error encoding audio frame");
    return 0;
  }
  ret = avcodec_receive_packet(aCoderContext_, pkt);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    return 0;
  }
  if (ret < 0) {
    ELOG_ERROR("error receiving encoded audio frame");
    return 0;
  }
  ELOG_DEBUG("Got OUTPUT");
  return pkt->size;
}

int AudioEncoder::closeEncoder() {
  if (aCoderContext_ != NULL) {
    avcodec_free_context(&aCoderContext_);
  }
  if (aFrame_ != NULL) {
    av_frame_free(&aFrame_);
  }
  return 0;
}


AudioDecoder::AudioDecoder() {
  aDecoder_ = NULL;
  aDecoderContext_ = NULL;
  dFrame_ = NULL;
}

AudioDecoder::~AudioDecoder() {
  ELOG_DEBUG("AudioDecoder Destructor");
  this->closeDecoder();
}

int AudioDecoder::initDecoder(const AudioCodecInfo& info) {
  aDecoder_ = avcodec_find_decoder(static_cast<AVCodecID>(info.codec));
  if (!aDecoder_) {
    ELOG_DEBUG("Audio decoder not found");
    return false;
  }

  aDecoderContext_ = avcodec_alloc_context3(aDecoder_);
  if (!aDecoderContext_) {
    ELOG_DEBUG("Error allocating audio decoder context");
    return false;
  }

  aDecoderContext_->sample_fmt = AV_SAMPLE_FMT_S16;
  aDecoderContext_->bit_rate = info.bitRate;
  aDecoderContext_->sample_rate = info.sampleRate;
  set_codec_channels(aDecoderContext_, 1);

  if (avcodec_open2(aDecoderContext_, aDecoder_, NULL) < 0) {
    ELOG_DEBUG("Error opening audio decoder");
    return false;
  }
  return true;
}

int AudioDecoder::initDecoder(AVCodecContext* context) {
  return 0;
}

int AudioDecoder::decodeAudio(unsigned char* inBuff, int inBuffLen,
    unsigned char* outBuff, int outBuffLen, int* gotFrame) {
  AVPacket* avpkt = av_packet_alloc();
  if (!avpkt) {
    return -1;
  }
  int outSize = 0;
  int decSize = 0;
  uint8_t *decBuff = reinterpret_cast<uint8_t*>(malloc(16000));
  if (!decBuff) {
    av_packet_free(&avpkt);
    return -1;
  }

  avpkt->data = (unsigned char*) inBuff;
  avpkt->size = inBuffLen;

  int ret = avcodec_send_packet(aDecoderContext_, avpkt);
  av_packet_free(&avpkt);
  if (ret < 0) {
    free(decBuff);
    return -1;
  }

  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    free(decBuff);
    return -1;
  }

  while ((ret = avcodec_receive_frame(aDecoderContext_, frame)) == 0) {
    int plane_size;
    int channels = get_codec_channels(aDecoderContext_);
    int data_size = av_samples_get_buffer_size(&plane_size,
        channels, frame->nb_samples,
        aDecoderContext_->sample_fmt, 1);
    outSize = 16000;
    if (outSize < data_size) {
      ELOG_DEBUG("output buffer size is too small for the current frame");
      av_frame_free(&frame);
      free(decBuff);
      return AVERROR(EINVAL);
    }

    memcpy(decBuff, frame->extended_data[0], plane_size);
    outSize = data_size;
    memcpy(outBuff, decBuff, outSize);
    outBuff += outSize;
    decSize += outSize;
  }

  av_frame_free(&frame);
  free(decBuff);

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    ELOG_DEBUG("Error al decodificar audio");
    return -1;
  }

  if (decSize <= 0) {
    ELOG_DEBUG("Error de decodificación de audio debido a tamaño incorrecto");
    return -1;
  }

  return decSize;
}

int AudioDecoder::closeDecoder() {
  if (aDecoderContext_ != NULL) {
    avcodec_free_context(&aDecoderContext_);
  }
  if (dFrame_ != NULL) {
    av_frame_free(&dFrame_);
  }
  return 0;
}
}  // namespace erizo
