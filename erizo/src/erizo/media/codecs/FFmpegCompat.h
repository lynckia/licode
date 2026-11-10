#ifndef ERIZO_SRC_ERIZO_MEDIA_CODECS_FFMPEGCOMPAT_H_
#define ERIZO_SRC_ERIZO_MEDIA_CODECS_FFMPEGCOMPAT_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
}

namespace erizo {

inline void set_codec_channels(AVCodecContext* ctx, int channels) {
#if LIBAVCODEC_VERSION_MAJOR >= 59
  av_channel_layout_default(&ctx->ch_layout, channels);
#else
  ctx->channels = channels;
  ctx->channel_layout = av_get_default_channel_layout(channels);
#endif
}

inline int get_codec_channels(const AVCodecContext* ctx) {
#if LIBAVCODEC_VERSION_MAJOR >= 59
  return ctx->ch_layout.nb_channels;
#else
  return ctx->channels;
#endif
}

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_MEDIA_CODECS_FFMPEGCOMPAT_H_
