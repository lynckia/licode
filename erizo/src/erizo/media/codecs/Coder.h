#ifndef ERIZO_SRC_ERIZO_MEDIA_CODECS_CODER_H_
#define ERIZO_SRC_ERIZO_MEDIA_CODECS_CODER_H_

#include <functional>
#include <boost/thread.hpp>
#include <chrono>  // NOLINT

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
typedef std::function<void(AVCodecContext *context, AVDictionary *dict)> InitContextBeforeOpenCB;

enum CoderOperationType {
  ENCODE_AV,
  DECODE_AV
};

class Coder {
  DECLARE_LOGGER();

 public:
  bool initContext(AVCodecContext **codec_ctx, AVCodec **av_codec, AVCodecID codec_id,
      CoderOperationType operation, const InitContextBeforeOpenCB callback);
  bool allocCodecContext(AVCodecContext **ctx, AVCodec **c, AVCodecID codec_id,
    CoderOperationType operation);
  bool openCodecContext(AVCodecContext *codec_ctx, AVCodec *codec, AVDictionary *opt);
  void logCodecContext(AVCodecContext *codec_ctx);
  bool decode(AVCodecContext *decode_ctx, AVFrame *frame, AVPacket *av_packet);
  void encode(AVCodecContext *encode_ctx, AVFrame *frame, AVPacket *av_packet,
      const EncodeCB &done);
  static void logAVStream(AVStream *av_stream);
};

class CoderCodec {
  DECLARE_LOGGER();

 public:
  CoderCodec();
  virtual ~CoderCodec();
  virtual int closeCodec();
  void logCodecContext();
  bool isInitialized();
  bool initializeStream(int index, AVFormatContext *f_context, AVStream **out_st);
  int getContextWidth();
  int getContextHeight();

 public:
  AVFrame* frame_;
  AVCodecContext* codec_context_;

 protected:
  Coder coder_;
  AVCodec* av_codec_;
  bool initialized_;
};

class CoderEncoder : public CoderCodec {
  DECLARE_LOGGER();

 public:
  bool initEncoder(const AVCodecID codec_id, const InitContextBeforeOpenCB callback);
  void encode(AVFrame *frame, AVPacket *av_packet, const EncodeCB &done);
};

class CoderDecoder : public CoderCodec {
  DECLARE_LOGGER();

 public:
  bool initDecoder(const AVCodecID codec_id, const InitContextBeforeOpenCB callback);
  bool decode(AVFrame *frame, AVPacket *av_packet);
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_CODECS_CODER_H_
