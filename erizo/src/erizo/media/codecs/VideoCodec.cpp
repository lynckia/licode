/**
 * VideoCodec.pp
 */
#include "media/codecs/VideoCodec.h"

#include <cstdio>
#include <cstring>
#include <string>

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(VideoEncoder, "media.codecs.VideoEncoder");
DEFINE_LOGGER(VideoDecoder, "media.codecs.VideoDecoder");

inline AVCodecID VideoCodecID2ffmpegDecoderID(VideoCodecID codec) {
  switch (codec) {
    case VIDEO_CODEC_H264: return AV_CODEC_ID_H264;
    case VIDEO_CODEC_VP8: return AV_CODEC_ID_VP8;
    case VIDEO_CODEC_MPEG4: return AV_CODEC_ID_MPEG4;
    default: return AV_CODEC_ID_VP8;
  }
}

int VideoEncoder::initEncoder(const VideoCodecInfo& info) {
  const InitContextCB callback = [info](AVCodecContext *context, AVDictionary *dict) {
    context->bit_rate = info.bitRate;
    context->rc_min_rate = info.bitRate;
    context->rc_max_rate = info.bitRate;  // VPX_CBR
    context->qmin = 0;
    context->qmax = 40;  // rc_quantifiers
    context->profile = 3;
    context->rc_initial_buffer_occupancy = 500;
    context->rc_buffer_size = 1000;
    context->width = info.width;
    context->height = info.height;
    context->pix_fmt = AV_PIX_FMT_YUV420P;
    context->time_base = (AVRational) {1, 90000};
    context->sample_aspect_ratio = (AVRational) { info.width, info.height };
    context->thread_count = 4;
  };

  AVCodecID av_codec_id = VideoCodecID2ffmpegDecoderID(info.codec);
  return this->initEncoder(av_codec_id, callback);
}

void VideoEncoder::encodeVideoBuffer(unsigned char* inBuffer, int len, unsigned char* outBuffer,
    const EncodeVideoBufferCB &done) {
  int size = codec_context_->width * codec_context_->height;

  frame_->pts = AV_NOPTS_VALUE;
  frame_->data[0] = inBuffer;
  frame_->data[1] = inBuffer + size;
  frame_->data[2] = inBuffer + size + size / 4;
  frame_->linesize[0] = codec_context_->width;
  frame_->linesize[1] = codec_context_->width / 2;
  frame_->linesize[2] = codec_context_->width / 2;

  AVPacket *av_packet =  av_packet_alloc();
  av_init_packet(av_packet);
  av_packet->data = outBuffer;

  EncodeCB done_callback = [done](AVPacket *pkt, AVFrame *frame, bool got_packet) {
    int len = pkt->size;
    av_frame_free(&frame);
    av_packet_free(&pkt);
    done(got_packet, len);
  };
  this->encode(frame_, av_packet, done_callback);
}

int VideoDecoder::initDecoder(const VideoCodecInfo& info) {
  const InitContextCB callback = [info](AVCodecContext *context, AVDictionary *dict) {
    context->width = info.width;
    context->height = info.height;
  };

  AVCodecID av_codec_id = VideoCodecID2ffmpegDecoderID(info.codec);
  return this->initDecoder(av_codec_id, callback);
}

int VideoDecoder::initDecoder(AVCodecParameters *codecpar) {
  const InitContextCB callback = [codecpar](AVCodecContext *context, AVDictionary *dict) {
    int error = avcodec_parameters_to_context(context, codecpar);
    if (error < 0) {
      ELOG_ERROR("Could not copy parameters to context.");
    }
  };

  return this->initDecoder(codecpar->codec_id, callback);
}

bool VideoDecoder::decode(AVFrame *frame, AVPacket *av_packet) {
  frame_->format = codec_context_->pix_fmt;
  frame_->width  = codec_context_->width;
  frame_->height = codec_context_->height;
  frame_->pts    = av_packet->pts;
  return CoderDecoder::decode(frame, av_packet);
}

int VideoDecoder::decodeVideoBuffer(unsigned char* inBuff, int inBuffLen,
    unsigned char* outBuff, int outBuffLen, int* gotFrame) {
  if (av_codec_ == 0 || codec_context_ == 0) {
    ELOG_DEBUG("Init Codec First");
    return -1;
  }

  *gotFrame = false;

  AVPacket avpkt;
  av_init_packet(&avpkt);

  avpkt.data = inBuff;
  avpkt.size = inBuffLen;

  if (avpkt.size > 0) {
    if (this->decode(frame_, &avpkt)) {
      *gotFrame = 1;
      goto decoding;
    }
  }

decoding:
  int outSize = codec_context_->height * codec_context_->width;

  if (outBuffLen < (outSize * 3 / 2)) {
    return outSize * 3 / 2;
  }

  unsigned char *lum = outBuff;
  unsigned char *cromU = outBuff + outSize;
  unsigned char *cromV = outBuff + outSize + outSize / 4;

  unsigned char *src = NULL;
  int src_linesize, dst_linesize;

  src_linesize = frame_->linesize[0];
  dst_linesize = codec_context_->width;
  src = frame_->data[0];

  for (int i = codec_context_->height; i > 0; i--) {
    memcpy(lum, src, dst_linesize);
    lum += dst_linesize;
    src += src_linesize;
  }

  src_linesize = frame_->linesize[1];
  dst_linesize = codec_context_->width / 2;
  src = frame_->data[1];

  for (int i = codec_context_->height / 2; i > 0; i--) {
    memcpy(cromU, src, dst_linesize);
    cromU += dst_linesize;
    src += src_linesize;
  }

  src_linesize = frame_->linesize[2];
  dst_linesize = codec_context_->width / 2;
  src = frame_->data[2];

  for (int i = codec_context_->height / 2; i > 0; i--) {
    memcpy(cromV, src, dst_linesize);
    cromV += dst_linesize;
    src += src_linesize;
  }
  av_packet_unref(&avpkt);

  return outSize * 3 / 2;
}
}  // namespace erizo
