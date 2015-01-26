/**
 * AudioCodec.pp
 */

#include "AudioCodec.h"

#include <cstdio>
#include <cstdlib>
#include <string.h>

namespace erizo {

  DEFINE_LOGGER(AudioEncoder, "media.codecs.AudioEncoder");
  DEFINE_LOGGER(AudioDecoder, "media.codecs.AudioDecoder");

  inline  AVCodecID
    AudioCodecID2ffmpegDecoderID(AudioCodecID codec)
    {
      switch (codec)
      {
        case AUDIO_CODEC_PCM_U8: return AV_CODEC_ID_PCM_U8;
        case AUDIO_CODEC_VORBIS: return AV_CODEC_ID_VORBIS;
        default: return AV_CODEC_ID_PCM_U8;
      }
    }

  AudioEncoder::AudioEncoder(){
    aCoder_ = NULL;
    aCoderContext_=NULL;
    aFrame_ = NULL;
  }

  AudioEncoder::~AudioEncoder(){
    ELOG_DEBUG("AudioEncoder Destructor");
    this->closeEncoder();
  }

  int AudioEncoder::initEncoder (const AudioCodecInfo& mediaInfo){

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
    //aCoderContext_->bit_rate = mediaInfo.bitRate;
    aCoderContext_->sample_rate = 8 /*mediaInfo.sampleRate*/;
    aCoderContext_->channels = 1;
    char errbuff[500];
    int res = avcodec_open2(aCoderContext_, aCoder_, NULL);
    if(res != 0){
      av_strerror(res, (char*)(&errbuff), 500);
      ELOG_DEBUG("fail when opening input %s", errbuff);
      return -1;
    }
    ELOG_DEBUG("Init audioEncoder end");
    return true;
  }

  int AudioEncoder::encodeAudio (unsigned char* inBuffer, int nSamples, AVPacket* pkt) {
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
      ELOG_ERROR("could not allocate audio frame");
      return 0;
    }
    int ret, got_output, buffer_size;

    frame->nb_samples = aCoderContext_->frame_size;
    frame->format = aCoderContext_->sample_fmt;
    //	frame->channel_layout = aCoderContext_->channel_layout;

    /* the codec gives us the frame size, in samples,
     * we calculate the size of the samples buffer in bytes */
    ELOG_DEBUG("channels %d, frame_size %d, sample_fmt %d",
        aCoderContext_->channels, aCoderContext_->frame_size,
        aCoderContext_->sample_fmt);
    buffer_size = av_samples_get_buffer_size(NULL, aCoderContext_->channels,
        aCoderContext_->frame_size, aCoderContext_->sample_fmt, 0);
    uint16_t* samples = (uint16_t*) malloc(buffer_size);
    if (!samples) {
      ELOG_ERROR("could not allocate %d bytes for samples buffer",buffer_size);
      return 0;
    }
    /* setup the data pointers in the AVFrame */
    ret = avcodec_fill_audio_frame(frame, aCoderContext_->channels,
        aCoderContext_->sample_fmt, (const uint8_t*) samples, buffer_size,
        0);
    if (ret < 0) {
        free(samples);
        ELOG_ERROR("could not setup audio frame");
        return 0;
    }

    ret = avcodec_encode_audio2(aCoderContext_, pkt, frame, &got_output);
    if (ret < 0) {
      ELOG_ERROR("error encoding audio frame");
      free(samples);
      return 0;
    }
    if (got_output) {
      //fwrite(pkt.data, 1, pkt.size, f);
      ELOG_DEBUG("Got OUTPUT");
    }

    return ret;
  }

  int AudioEncoder::closeEncoder (){
    if (aCoderContext_!=NULL){
      avcodec_close(aCoderContext_);
    }
    if (aFrame_!=NULL){
      av_frame_free(&aFrame_);
    }
    return 0;
  }


  AudioDecoder::AudioDecoder(){
    aDecoder_ = NULL;
    aDecoderContext_ = NULL;
    dFrame_ = NULL;
  }

  AudioDecoder::~AudioDecoder(){
    ELOG_DEBUG("AudioDecoder Destructor");
    this->closeDecoder();
  }

  int AudioDecoder::initDecoder (const AudioCodecInfo& info){
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
    aDecoderContext_->channels = 1;

    if (avcodec_open2(aDecoderContext_, aDecoder_, NULL) < 0) {
      ELOG_DEBUG("Error opening audio decoder");
      return false;
    }
    return true;
  }

  int AudioDecoder::initDecoder (AVCodecContext* context){
    return 0;
  }
  int AudioDecoder::decodeAudio(unsigned char* inBuff, int inBuffLen,
      unsigned char* outBuff, int outBuffLen, int* gotFrame){


    AVPacket avpkt;
    int outSize;
    int decSize = 0;
    int len = -1;
    uint8_t *decBuff = (uint8_t*) malloc(16000);

    av_init_packet(&avpkt);
    avpkt.data = (unsigned char*) inBuff;
    avpkt.size = inBuffLen;

    while (avpkt.size > 0) {

      outSize = 16000;

      //Puede fallar. Cogido de libavcodec/utils.c del paso de avcodec_decode_audio3 a avcodec_decode_audio4
      //avcodec_decode_audio3(aDecoderContext, (short*)decBuff, &outSize, &avpkt);

      AVFrame frame;
      int got_frame = 0;

      //      aDecoderContext->get_buffer = avcodec_default_get_buffer;
      //      aDecoderContext->release_buffer = avcodec_default_release_buffer;

      len = avcodec_decode_audio4(aDecoderContext_, &frame, &got_frame,
          &avpkt);
      if (len >= 0 && got_frame) {
        int plane_size;
        //int planar = av_sample_fmt_is_planar(aDecoderContext->sample_fmt);
        int data_size = av_samples_get_buffer_size(&plane_size,
            aDecoderContext_->channels, frame.nb_samples,
            aDecoderContext_->sample_fmt, 1);
        if (outSize < data_size) {
          ELOG_DEBUG("output buffer size is too small for the current frame");
          free(decBuff);
          return AVERROR(EINVAL);
        }

        memcpy(decBuff, frame.extended_data[0], plane_size);

        /* Si hay más de un canal
           if (planar && aDecoderContext->channels > 1) {
           uint8_t *out = ((uint8_t *)decBuff) + plane_size;
           for (int ch = 1; ch < aDecoderContext->channels; ch++) {
           memcpy(out, frame.extended_data[ch], plane_size);
           out += plane_size;
           }
           }
           */
        outSize = data_size;
      } else {
        outSize = 0;
      }

      if (len < 0) {
        ELOG_DEBUG("Error al decodificar audio");
        free(decBuff);
        return -1;
      }

      avpkt.size -= len;
      avpkt.data += len;

      if (outSize <= 0) {
        continue;
      }

      memcpy(outBuff, decBuff, outSize);
      outBuff += outSize;
      decSize += outSize;
    }

    free(decBuff);

    if (outSize <= 0) {
      ELOG_DEBUG("Error de decodificación de audio debido a tamaño incorrecto");
      return -1;
    }

    return decSize;
  }
  int AudioDecoder::closeDecoder(){
    if (aDecoderContext_!=NULL){
      avcodec_close(aDecoderContext_);
    }
    if (dFrame_!=NULL){
      av_frame_free(&dFrame_);
    }
    return 0;
  }
}
