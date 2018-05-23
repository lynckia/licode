#ifndef ERIZO_SRC_ERIZO_MEDIA_CODECS_CODER_H_
#define ERIZO_SRC_ERIZO_MEDIA_CODECS_CODER_H_

#include <functional>
#include <boost/thread.hpp>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libavresample/avresample.h>
}

#include "./logger.h"

inline static const std::string av_make_error_string_cpp(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE];
  av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
  return (std::string)errbuf;
}

#define av_err2str_cpp(errnum) av_make_error_string_cpp(errnum).c_str()

namespace erizo {

typedef std::function<void(AVPacket *packet, AVFrame *frame, bool success)> EncodeCB;

enum CoderOperationType {
  OPERATION_ENCODE,
  OPERATION_DECODE
};

class Coder {
  DECLARE_LOGGER();

 public:
  Coder();
  virtual ~Coder();
  bool allocCodecContext(AVCodecContext **ctx, AVCodec **c, AVCodecID codec_id,
    CoderOperationType operation);
  bool openCodecContext(AVCodecContext *codec_ctx, AVCodec *codec, AVDictionary *opt);
  void logCodecContext(AVCodecContext *codec_ctx);
  bool decode(AVCodecContext *decode_ctx, AVFrame *frame, AVPacket *av_packet);
  void encode(AVCodecContext *encode_ctx, AVFrame *frame, AVPacket *av_packet,
      const EncodeCB &done);
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_CODECS_CODER_H_
