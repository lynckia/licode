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

VideoEncoder::VideoEncoder() {
  avcodec_register_all();
  av_codec = NULL;
  encode_context_ = NULL;
  cPicture = NULL;
  initialized = false;
}

VideoEncoder::~VideoEncoder() {
  this->closeEncoder();
}

int VideoEncoder::initEncoder(const VideoCodecInfo& info) {
  if (!coder_.allocCodecContext(&encode_context_, &av_codec,
      VideoCodecID2ffmpegDecoderID(info.codec), OPERATION_ENCODE)) {
    return -1;
  }
  encode_context_->bit_rate = info.bitRate;
  encode_context_->rc_min_rate = info.bitRate;
  encode_context_->rc_max_rate = info.bitRate;  // VPX_CBR
  encode_context_->qmin = 0;
  encode_context_->qmax = 40;  // rc_quantifiers
  encode_context_->profile = 3;
  encode_context_->rc_initial_buffer_occupancy = 500;

  encode_context_->rc_buffer_size = 1000;

  encode_context_->width = info.width;
  encode_context_->height = info.height;
  encode_context_->pix_fmt = AV_PIX_FMT_YUV420P;
  encode_context_->time_base = (AVRational) {1, 90000};

  encode_context_->sample_aspect_ratio = (AVRational) { info.width, info.height };
  encode_context_->thread_count = 4;

  if (!coder_.openCodecContext(encode_context_, av_codec, NULL)) {
    return -2;
  }

  cPicture = av_frame_alloc();
  if (!cPicture) {
    ELOG_DEBUG("Error allocating video frame");
    return -3;
  }

  ELOG_DEBUG("VideoEncoder configured successfully %d x %d",
      encode_context_->width, encode_context_->height);
  initialized = true;
  return 0;
}

void VideoEncoder::encodeVideoBuffer(unsigned char* inBuffer, int len, unsigned char* outBuffer, const EncodeVideoBufferCB &done) {
  int size = encode_context_->width * encode_context_->height;

  cPicture->pts = AV_NOPTS_VALUE;
  cPicture->data[0] = inBuffer;
  cPicture->data[1] = inBuffer + size;
  cPicture->data[2] = inBuffer + size + size / 4;
  cPicture->linesize[0] = encode_context_->width;
  cPicture->linesize[1] = encode_context_->width / 2;
  cPicture->linesize[2] = encode_context_->width / 2;

  AVPacket *av_packet =  av_packet_alloc();
  av_init_packet(av_packet);
  av_packet->data = outBuffer;

  EncodeCB done_callback = [done](AVPacket *pkt, AVFrame *frame, bool got_packet) {
    int len = pkt->size;
    av_frame_free(&frame);
    av_packet_free(&pkt);
    done(got_packet, len);
  };
  coder_.encode(encode_context_, cPicture, av_packet, done_callback);
}

int VideoEncoder::closeEncoder() {
  if (encode_context_ != NULL)
    avcodec_close(encode_context_);
  if (cPicture != NULL)
    av_frame_free(&cPicture);
  initialized = false;
  return 0;
}


VideoDecoder::VideoDecoder() {
  avcodec_register_all();
  av_codec = NULL;
  decode_context_ = NULL;
  dPicture = NULL;
  initWithContext_ = false;
  initialized = false;
}

VideoDecoder::~VideoDecoder() {
  this->closeDecoder();
}

int VideoDecoder::initDecoder(const VideoCodecInfo& info) {
  ELOG_DEBUG("Init Decoder");
  if (!coder_.allocCodecContext(&decode_context_, &av_codec,
      VideoCodecID2ffmpegDecoderID(info.codec), OPERATION_DECODE)) {
    return -1;
  }

  decode_context_->width = info.width;
  decode_context_->height = info.height;

  if (!coder_.openCodecContext(decode_context_, av_codec, NULL)) {
    return -2;
  }

  dPicture = av_frame_alloc();
  if (!dPicture) {
    ELOG_DEBUG("Error allocating video frame");
    return -3;
  }

  initialized = true;
  return 0;
}

int VideoDecoder::initDecoder(AVCodecContext** context, AVCodecParameters *codecpar) {
  int error;
  AVCodecContext *c;

  ELOG_DEBUG("Init Decoder context");
  initWithContext_ = true;

  if (!coder_.allocCodecContext(&c, &av_codec,
      codecpar->codec_id, OPERATION_DECODE)) {
    return -1;
  }

  error = avcodec_parameters_to_context(c, codecpar);
  if (error < 0) {
    ELOG_ERROR("Could not copy parameters to context.");
    return -2;
  }

  if (!coder_.openCodecContext(decode_context_, av_codec, NULL)) {
    return -3;
  }

  *context = c;
  decode_context_ = *context;

  dPicture = av_frame_alloc();
  if (!dPicture) {
    ELOG_DEBUG("Error allocating video frame");
    return -4;
  }

  initialized = true;
  return 0;
}

int VideoDecoder::decodeVideoBuffer(unsigned char* inBuff, int inBuffLen,
    unsigned char* outBuff, int outBuffLen, int* gotFrame) {
  if (av_codec == 0 || decode_context_ == 0) {
    ELOG_DEBUG("Init Codec First");
    return -1;
  }

  *gotFrame = false;

  AVPacket avpkt;
  av_init_packet(&avpkt);

  avpkt.data = inBuff;
  avpkt.size = inBuffLen;

  if (avpkt.size > 0) {
    if (coder_.decode(decode_context_, dPicture, &avpkt)) {
      *gotFrame = 1;
      goto decoding;
    }
  }

decoding:
  int outSize = decode_context_->height * decode_context_->width;

  if (outBuffLen < (outSize * 3 / 2)) {
    return outSize * 3 / 2;
  }

  unsigned char *lum = outBuff;
  unsigned char *cromU = outBuff + outSize;
  unsigned char *cromV = outBuff + outSize + outSize / 4;

  unsigned char *src = NULL;
  int src_linesize, dst_linesize;

  src_linesize = dPicture->linesize[0];
  dst_linesize = decode_context_->width;
  src = dPicture->data[0];

  for (int i = decode_context_->height; i > 0; i--) {
    memcpy(lum, src, dst_linesize);
    lum += dst_linesize;
    src += src_linesize;
  }

  src_linesize = dPicture->linesize[1];
  dst_linesize = decode_context_->width / 2;
  src = dPicture->data[1];

  for (int i = decode_context_->height / 2; i > 0; i--) {
    memcpy(cromU, src, dst_linesize);
    cromU += dst_linesize;
    src += src_linesize;
  }

  src_linesize = dPicture->linesize[2];
  dst_linesize = decode_context_->width / 2;
  src = dPicture->data[2];

  for (int i = decode_context_->height / 2; i > 0; i--) {
    memcpy(cromV, src, dst_linesize);
    cromV += dst_linesize;
    src += src_linesize;
  }
  av_packet_unref(&avpkt);

  return outSize * 3 / 2;
}

int VideoDecoder::closeDecoder() {
  if (!initWithContext_ && decode_context_ != NULL)
    avcodec_close(decode_context_);
  if (dPicture != NULL)
    av_frame_free(&dPicture);
  initialized = false;
  return 0;
}

}  // namespace erizo
