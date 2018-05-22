#include "media/codecs/Coder.h"

namespace erizo {

DEFINE_LOGGER(Coder, "media.codecs.Coder");

Coder::Coder() {
}

Coder::~Coder() {
}

bool Coder::allocCodecContext(AVCodecContext **ctx, AVCodec **c, AVCodecID codec_id,
    CoderOperationType operation) {
  AVCodec *codec = nullptr;
  AVCodecContext *codec_ctx = *ctx;
  if (operation == OPERATION_ENCODE) {
    codec = avcodec_find_encoder(codec_id);
  } else {
    codec = avcodec_find_decoder(codec_id);
  }
  if (codec == nullptr) {
    ELOG_ERROR("Cannot find codec %d", codec_id);
    return false;
  }
  codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
      ELOG_ERROR("Could not allocate codec context for %s", codec->name);
      return false;
  } else {
    ELOG_INFO("Successfully allocated context for codec %s.", codec->name);
  }
  *c = codec;
  *ctx = codec_ctx;
  return true;
}

bool Coder::openCodecContext(AVCodecContext *codec_ctx, AVCodec *codec, AVDictionary *opt) {
  const char *operation = (av_codec_is_encoder(codec_ctx->codec) ? "encoder" : "decoder");
  int ret = avcodec_open2(codec_ctx, NULL, &opt);
  if (ret < 0) {
      static char error_str[255];
      av_strerror(ret, error_str, sizeof(error_str));
      ELOG_ERROR("Could not open %s codec of %s: %s", operation,
          codec->name,
          error_str);
  }
  av_dict_free(&opt);
  this->logCodecContext(codec_ctx);
  return ret < 0 ? false : true;
}

void Coder::logCodecContext(AVCodecContext *codec_ctx) {
  const char *operation = (av_codec_is_encoder(codec_ctx->codec) ? "encoder" : "decoder");
  if (codec_ctx->codec->type == AVMEDIA_TYPE_AUDIO) {
    ELOG_DEBUG("\nAudio %s codec: %s \nchannel_layout: %d\nchannels: %d\nframe_size: %d\nsample_rate: %d\nsample_fmt: %s\nbits per sample: %d",
        operation, codec_ctx->codec->name, codec_ctx->channel_layout, codec_ctx->channels, codec_ctx->frame_size,
        codec_ctx->sample_rate, av_get_sample_fmt_name(codec_ctx->sample_fmt),
        av_get_bytes_per_sample (codec_ctx->sample_fmt));
  } else if (codec_ctx->codec->type == AVMEDIA_TYPE_VIDEO) {
    ELOG_DEBUG("\nVideo %s codec: %s\n framerate: {%d/%d}\n time_base: {%d/%d}\n",
        operation, codec_ctx->codec->name, codec_ctx->framerate.num, codec_ctx->framerate.den,
        codec_ctx->time_base.num, codec_ctx->time_base.den);
  }
}

bool Coder::decode(AVCodecContext *decode_ctx, AVFrame *frame, AVPacket *av_packet) {
  int ret;
  ret = avcodec_send_packet(decode_ctx, av_packet);
  if (ret < 0) {
    ELOG_ERROR("avcodec_send_packet failed. %d %s", ret, av_err2str(ret));
    return false;
  }
  ret = avcodec_receive_frame(decode_ctx, frame);
  if (ret != 0) {
    ELOG_ERROR("avcodec_receive_frame. %d %s", ret, av_err2str(ret));
    return false;
  }
  ELOG_DEBUG("decoded %s, nb_samples: %d, size: %d", decode_ctx->codec->name,
      frame->nb_samples, av_packet->size);
  return true;
}

void Coder::encode(AVCodecContext *encode_ctx, AVFrame *frame, AVPacket *av_packet, const EncodeCB &done) {
  auto t_started = std::chrono::high_resolution_clock::now();
  int ret = avcodec_send_frame(encode_ctx, frame);
  if (ret < 0) {
    ELOG_ERROR("avcodec_send_frame failed for %s. %d %s", encode_ctx->codec->name,
        ret, av_err2str(ret));
    done(av_packet, frame, false);
  }
  while(ret >= 0) {
    ret = avcodec_receive_packet(encode_ctx, av_packet);
    if (ret == AVERROR_EOF) {
      ELOG_DEBUG("avcodec_receive_packet AVERROR_EOF, %s, ret: %d, %s", encode_ctx->codec->name,
          ret, av_err2str(ret))
      done(av_packet, frame, false);
      return;
    } else if (ret == AVERROR(EAGAIN)) {
      done(av_packet, frame, false);
      return;
    } else if (ret == 0) {
      auto t_done = std::chrono::high_resolution_clock::now();
      ELOG_DEBUG("Encoding %s time: %d milliseconds", encode_ctx->codec->name,
          std::chrono::duration_cast<std::chrono::milliseconds>(t_done-t_started).count());
      done(av_packet, frame, true);
    } else {
      ELOG_ERROR("avcodec_receive_packet failed. %s, %d %s", encode_ctx->codec->name,
          ret, av_err2str(ret));
      done(av_packet, frame, false);
    }
  }
}

}  // namespace erizo
