#include "media/codecs/Coder.h"

namespace erizo {

DEFINE_LOGGER(Coder,        "media.codecs.Coder");
DEFINE_LOGGER(CoderCodec,   "media.codecs.CoderCodec");
DEFINE_LOGGER(CoderEncoder, "media.codecs.CoderEncoder");
DEFINE_LOGGER(CoderDecoder, "media.codecs.CoderDecoder");

bool Coder::initContext(AVCodecContext **codec_ctx, AVCodec **av_codec, AVCodecID codec_id,
    CoderOperationType operation, InitContextBeforeOpenCB callback) {
  bool success = false;
  AVDictionary *opt = NULL;
  const char *codec_name = avcodec_get_name(codec_id);
  const char *codec_for = operation == ENCODE_AV ? "encode" : "decode";
  if (this->allocCodecContext(codec_ctx, av_codec, codec_id, operation)) {
    callback(*codec_ctx, opt);
    success = this->openCodecContext(*codec_ctx, *av_codec, opt);
    if (success) {
      ELOG_INFO("Successfully initialized %s context for %s.", codec_for, codec_name);
    } else {
      ELOG_ERROR("Could not initialize %s context for %s.", codec_for, codec_name);
    }
  }
  av_dict_free(&opt);
  return success;
}

bool Coder::allocCodecContext(AVCodecContext **ctx, AVCodec **c, AVCodecID codec_id,
    CoderOperationType operation) {
  AVCodec *codec;
  AVCodecContext *codec_ctx;
  const char *codec_for;
  if (operation == ENCODE_AV) {
    codec = avcodec_find_encoder(codec_id);
    codec_for = "encoding";
  } else {
    codec = avcodec_find_decoder(codec_id);
    codec_for = "decoding";
  }
  if (!codec) {
    ELOG_ERROR("Cannot find codec %d for %s", codec_id, codec_for);
    return false;
  }
  codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
      ELOG_ERROR("Could not allocate codec context for %s", codec->name);
      return false;
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
    ELOG_DEBUG("\nAudio %s codec: %s \nchannel_layout: %d\nchannels: %d\nframe_size: %d\ntime_base: %d/%d\n,"
        "sample_rate: %d\nsample_fmt: %s\nbits per sample: %d",
        operation, codec_ctx->codec->name, codec_ctx->channel_layout, codec_ctx->channels, codec_ctx->frame_size,
        codec_ctx->time_base.num, codec_ctx->time_base.den, codec_ctx->sample_rate,
        av_get_sample_fmt_name(codec_ctx->sample_fmt), av_get_bytes_per_sample(codec_ctx->sample_fmt));
  } else if (codec_ctx->codec->type == AVMEDIA_TYPE_VIDEO) {
    ELOG_DEBUG("\nVideo %s codec: %s\n framerate: {%d/%d}\n time_base: {%d/%d}\n",
        operation, codec_ctx->codec->name, codec_ctx->framerate.num, codec_ctx->framerate.den,
        codec_ctx->time_base.num, codec_ctx->time_base.den);
  }
}

void Coder::logAVStream(AVStream *st) {
  ELOG_DEBUG("Stream idx: %d id: %d: codec: %s, start: %d, time_base: %d/%d",
      st->index, st->id, avcodec_get_name(st->codecpar->codec_id), st->start_time,
      st->time_base.num, st->time_base.den);
}

bool Coder::decode(AVCodecContext *decode_ctx, AVFrame *frame, AVPacket *av_packet) {
  int ret;
  ret = avcodec_send_packet(decode_ctx, av_packet);
  if (ret < 0) {
    ELOG_ERROR("avcodec_send_packet failed. %d %s", ret, av_err2str_cpp(ret));
    return false;
  }
  ret = avcodec_receive_frame(decode_ctx, frame);
  if (ret != 0) {
    ELOG_ERROR("avcodec_receive_frame. %d %s", ret, av_err2str_cpp(ret));
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
        ret, av_err2str_cpp(ret));
    done(av_packet, frame, false);
  }
  while (ret >= 0) {
    ret = avcodec_receive_packet(encode_ctx, av_packet);
    if (ret == AVERROR_EOF) {
      ELOG_DEBUG("avcodec_receive_packet AVERROR_EOF, %s, ret: %d, %s", encode_ctx->codec->name,
          ret, av_err2str_cpp(ret))
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
          ret, av_err2str_cpp(ret));
      done(av_packet, frame, false);
    }
  }
}

CoderCodec::CoderCodec() {
  av_register_all();
  avcodec_register_all();
  av_codec_       = nullptr;
  codec_context_  = nullptr;
  initialized_    = false;
  frame_          = av_frame_alloc();
  if (!frame_) {
    ELOG_ERROR("Error allocating encode frame");
  }
}

CoderCodec::~CoderCodec() {
  this->closeCodec();
}

void CoderCodec::logCodecContext() {
  coder_.logCodecContext(codec_context_);
}

bool CoderCodec::isInitialized() {
  return initialized_;
}

bool CoderCodec::initializeStream(int index, AVFormatContext *f_context, AVStream **out_st) {
  if (!this->isInitialized()) {
    ELOG_ERROR("You are trying to initialize a stream with an uninitialized codec.");
    return false;
  }
  AVStream *st;
  const char *codec_name = av_codec_->name;
  st = avformat_new_stream(f_context, av_codec_);
  if (!st) {
    ELOG_ERROR("Could not create stream for %s.", codec_name);
  } else {
    int ret = avcodec_parameters_from_context(st->codecpar, codec_context_);
    if (ret != 0) {
      ELOG_ERROR("Could not copy codec paramaters for %s.", codec_name);
    } else {
      st->id = index;
      st->time_base = codec_context_->time_base;
      ELOG_INFO("Created stream for codec: %s with index: %d", codec_name, index);
      *out_st = st;
      return true;
    }
  }
  return false;
}

int CoderCodec::getContextWidth() {
  return codec_context_->width;
}

int CoderCodec::getContextHeight() {
  return codec_context_->height;
}

int CoderCodec::closeCodec() {
  ELOG_DEBUG("Closing CoderCodec.");
  if (codec_context_ != nullptr) {
    avcodec_close(codec_context_);
    avcodec_free_context(&codec_context_);
  }
  if (frame_ != nullptr) {
    av_frame_free(&frame_);
  }
  initialized_ = false;
  return 0;
}

bool CoderEncoder::initEncoder(const AVCodecID codec_id, const InitContextBeforeOpenCB callback) {
  if (coder_.initContext(&codec_context_, &av_codec_, codec_id, ENCODE_AV, callback)) {
    initialized_ = true;
  }
  return initialized_;
}

void CoderEncoder::encode(AVFrame *frame, AVPacket *av_packet, const EncodeCB &done) {
  coder_.encode(codec_context_, frame, av_packet, done);
}

bool CoderDecoder::initDecoder(const AVCodecID codec_id, const InitContextBeforeOpenCB callback) {
  if (coder_.initContext(&codec_context_, &av_codec_, codec_id, DECODE_AV, callback)) {
    initialized_ = true;
  }
  return initialized_;
}

bool CoderDecoder::decode(AVFrame *frame, AVPacket *av_packet) {
  return coder_.decode(codec_context_, frame, av_packet);
}
}  // namespace erizo
