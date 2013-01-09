/**
 * VP8Codec.pp
 */

#include "VideoCodec.h"

#include <string>

#include <cstddef>
#include <cstdio>
#include <string.h>
#include <boost/cstdint.hpp>

namespace erizo {
  inline  CodecID
    VideoCodecID2ffmpegDecoderID(VideoCodecID codec)
    {
      switch (codec)
      {
        case VIDEO_CODEC_H264: return CODEC_ID_H264;
        case VIDEO_CODEC_VP8: return CODEC_ID_VP8;
        case VIDEO_CODEC_MPEG4: return CODEC_ID_MPEG4;
        default: printf("Unknown codec\n"); return CODEC_ID_VP8;
      }
    }

  VideoEncoder::VideoEncoder(){
    avcodec_register_all();
  }

  int VideoEncoder::initEncoder(const VideoCodecInfo& info){
    vCoder = avcodec_find_encoder(VideoCodecID2ffmpegDecoderID(info.codec));
    if (!vCoder) {
      printf("Video codec not found for encoder");
      return -1;
    }

    vCoderContext = avcodec_alloc_context3(vCoder);
    if (!vCoderContext) {
      printf("Error allocating vCoderContext");
      return -2;
    }

    vCoderContext->bit_rate = info.bitRate;
//    vCoderContext->rc_min_rate = info.bitRate; //
//    vCoderContext->rc_max_rate = info.bitRate; // VPX_CBR
//    vCoderContext->qmin = 8;
//    vCoderContext->qmax = 56; // rc_quantifiers
    vCoderContext->profile = 3;
    //    vCoderContext->frame_skip_threshold = 30;
    //   vCoderContext->rc_buffer_aggressivity = 1;
    //    vCoderContext->rc_buffer_size = vCoderContext->bit_rate;
    //    vCoderContext->rc_initial_buffer_occupancy = vCoderContext->bit_rate / 2;
    vCoderContext->width = info.width;
    vCoderContext->height = info.height;
    vCoderContext->pix_fmt = PIX_FMT_YUV420P;
    vCoderContext->time_base = (AVRational) {1, 90000};
    //
    vCoderContext->sample_aspect_ratio =
      (AVRational) {info.width,info.height};
    vCoderContext->thread_count = 4;

    if (avcodec_open2(vCoderContext, vCoder, NULL) < 0) {
      printf("Error opening video decoder");
      return -3;
    }

    cPicture = avcodec_alloc_frame();
    if (!cPicture) {
      printf("Error allocating video frame");
      return -4;
    }

    printf("videoCoder configured successfully %d x %d\n", vCoderContext->width,
        vCoderContext->height);
    return 0;
  }

  int VideoEncoder::encodeVideo (unsigned char* inBuffer, int inLength, unsigned char* outBuffer, int outLength, int& hasFrame){

    int size = vCoderContext->width * vCoderContext->height;
    //    printf("vCoderContext width %d\n", vCoderContext->width);

    cPicture->pts = AV_NOPTS_VALUE;
    cPicture->data[0] = inBuffer;
    cPicture->data[1] = inBuffer + size;
    cPicture->data[2] = inBuffer + size + size / 4;
    cPicture->linesize[0] = vCoderContext->width;
    cPicture->linesize[1] = vCoderContext->width / 2;
    cPicture->linesize[2] = vCoderContext->width / 2;

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = outBuffer;
    pkt.size = outLength;

    int ret = 0;
    int got_packet = 0;
    //    printf(
    //        "Before encoding inBufflen %d, size %d, codecontext width %d pkt->size%d\n",
    //        inLength, size, vCoderContext->width, pkt.size);
    ret = avcodec_encode_video2(vCoderContext, &pkt, cPicture, &got_packet);
    //    printf("Encoded video size %u, ret %d, got_packet %d, pts %lld, dts %lld\n",
    //        pkt.size, ret, got_packet, pkt.pts, pkt.dts);
    if (!ret && got_packet && vCoderContext->coded_frame) {
      vCoderContext->coded_frame->pts = pkt.pts;
      vCoderContext->coded_frame->key_frame =
        !!(pkt.flags & AV_PKT_FLAG_KEY);
    }
    return ret ? ret : pkt.size;
  }

  int VideoEncoder::closeEncoder() {
    return 0;
  }


  VideoDecoder::VideoDecoder(){
    avcodec_register_all();
    vDecoder = 0;
    vDecoderContext = 0;
  }

  int VideoDecoder::initDecoder (const VideoCodecInfo& info){
    printf("Init Decoder\n");
    vDecoder = avcodec_find_decoder(VideoCodecID2ffmpegDecoderID(info.codec));
    if (!vDecoder) {
      printf("Error getting video decoder\n");
      return -1;
    }

    vDecoderContext = avcodec_alloc_context3(vDecoder);
    if (!vDecoderContext) {
      printf("Error getting allocating decoder context");
      return -1;
    }

    vDecoderContext->width = info.width;
    vDecoderContext->height = info.height;

    if (avcodec_open2(vDecoderContext, vDecoder, NULL) < 0) {
      printf("Error opening video decoder\n");
      return -1;
    }

    dPicture = avcodec_alloc_frame();
    if (!dPicture) {
      printf("Error allocating video frame\n");
      return -1;
    }

    return 0;
  }
  int VideoDecoder::decodeVideo(unsigned char* inBuff, int inBuffLen,
      unsigned char* outBuff, int outBuffLen, int* gotFrame){
    if (vDecoder == 0 || vDecoderContext == 0){
      printf("Init Codec First\n");
      return -1;
    }

    *gotFrame = false;

    AVPacket avpkt;
    av_init_packet(&avpkt);

    avpkt.data = inBuff;
    avpkt.size = inBuffLen;

    int got_picture;
    int len;

    while (avpkt.size > 0) {

      len = avcodec_decode_video2(vDecoderContext, dPicture, &got_picture,
          &avpkt);

      if (len < 0) {
        printf("Error decoding video frame\n");
        return -1;
      }

      if (got_picture) {
        *gotFrame = 1;
        goto decoding;
      }
      avpkt.size -= len;
      avpkt.data += len;
    }

    if (!got_picture) {
      return -1;
    }

decoding:

    int outSize = vDecoderContext->height * vDecoderContext->width;

    if (outBuffLen < (outSize * 3 / 2)) {
      return outSize * 3 / 2;
    }

    unsigned char *lum = outBuff;
    unsigned char *cromU = outBuff + outSize;
    unsigned char *cromV = outBuff + outSize + outSize / 4;

    unsigned char *src = NULL;
    int src_linesize, dst_linesize;

    src_linesize = dPicture->linesize[0];
    dst_linesize = vDecoderContext->width;
    src = dPicture->data[0];

    for (int i = vDecoderContext->height; i > 0; i--) {
      memcpy(lum, src, dst_linesize);
      lum += dst_linesize;
      src += src_linesize;
    }

    src_linesize = dPicture->linesize[1];
    dst_linesize = vDecoderContext->width / 2;
    src = dPicture->data[1];

    for (int i = vDecoderContext->height / 2; i > 0; i--) {
      memcpy(cromU, src, dst_linesize);
      cromU += dst_linesize;
      src += src_linesize;
    }

    src_linesize = dPicture->linesize[2];
    dst_linesize = vDecoderContext->width / 2;
    src = dPicture->data[2];

    for (int i = vDecoderContext->height / 2; i > 0; i--) {
      memcpy(cromV, src, dst_linesize);
      cromV += dst_linesize;
      src += src_linesize;
    }
    av_free_packet(&avpkt);

    return outSize * 3 / 2;
  }

  int VideoDecoder::closeDecoder(){
    if (dPicture!=0)
      //      av_free(dPicture);
      if (vDecoderContext!=0){
        avcodec_close(vDecoderContext);
        //      av_free(vDecoderContext);
      }
    return 0;
  }

}
