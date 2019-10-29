#ifndef ERIZO_SRC_ERIZO_MEDIA_MEDIAPROCESSOR_H_
#define ERIZO_SRC_ERIZO_MEDIA_MEDIAPROCESSOR_H_

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <string>

#include "rtp/RtpVP8Parser.h"
#include "./MediaDefinitions.h"
#include "codecs/Codecs.h"
#include "codecs/VideoCodec.h"
#include "./logger.h"

namespace erizo {
struct RTPInfo {
  enum AVCodecID codec;
  unsigned int ssrc;
  unsigned int PT;
};

enum ProcessorType {
  RTP_ONLY, AVF, PACKAGE_ONLY
};


enum DataType {
  VIDEO, AUDIO
};

struct RawDataPacket {
  unsigned char* data;
  int length;
  DataType type;
};

struct MediaInfo {
  std::string url;
  bool hasVideo;
  bool hasAudio;
  ProcessorType processorType;
  RTPInfo rtpVideoInfo;
  RTPInfo rtpAudioInfo;
  VideoCodecInfo videoCodec;
  AudioCodecInfo audioCodec;
};

#define UNPACKAGED_BUFFER_SIZE 150000
#define PACKAGED_BUFFER_SIZE 2000
// class MediaProcessor{
// MediaProcessor();
// virtual ~Mediaprocessor();
// private:
// InputProcessor* input;
// OutputProcessor* output;
// };

class RawDataReceiver {
 public:
  virtual void receiveRawData(const RawDataPacket& packet) = 0;
  virtual ~RawDataReceiver() {}
};

class RTPDataReceiver {
 public:
  virtual void receiveRtpData(unsigned char* rtpdata, int len) = 0;
  virtual ~RTPDataReceiver() {}
};

class RTPSink;

class InputProcessor: public MediaSink {
  DECLARE_LOGGER();

 public:
  InputProcessor();
  virtual ~InputProcessor();

  int init(const MediaInfo& info, RawDataReceiver* receiver);


  int unpackageVideo(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff, int* gotFrame);

  int unpackageAudio(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff);

  void closeSink();
  boost::future<void> close() override;

 private:
  int audioDecoder;
  int videoDecoder;

  double lastVideoTs_;

  MediaInfo mediaInfo;

  int audioUnpackager;
  int videoUnpackager;

  int gotUnpackagedFrame_;
  int upackagedSize_;

  unsigned char* decodedBuffer_;
  unsigned char* unpackagedBuffer_;
  unsigned char* unpackagedBufferPtr_;

  unsigned char* decodedAudioBuffer_;
  unsigned char* unpackagedAudioBuffer_;

  AVCodec* aDecoder;
  AVCodecContext* aDecoderContext;

  VideoDecoder vDecoder;

  RawDataReceiver* rawReceiver_;

  erizo::RtpVP8Parser pars;

  bool initAudioDecoder();

  bool initAudioUnpackager();
  bool initVideoUnpackager();
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override;
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override;
  int deliverEvent_(MediaEventPtr event) override;

  int decodeAudio(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff);
};

class OutputProcessor: public RawDataReceiver {
  DECLARE_LOGGER();

 public:
  OutputProcessor();
  virtual ~OutputProcessor();
  int init(const MediaInfo& info, RTPDataReceiver* rtpReceiver);
  void close();
  void receiveRawData(const RawDataPacket& packet);

  void requestKeyframe();

  void setTargetBitrate(uint64_t bitrate);

  int packageAudio(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff, long int pts = 0);  // NOLINT

  int packageVideo(unsigned char* inBuff, int buffSize, unsigned char* outBuff, long int pts = 0);  // NOLINT

 private:
  int audioCoder;
  int videoCoder;

  int audioPackager;
  int videoPackager;

  unsigned int seqnum_;
  unsigned int audioSeqnum_;

  unsigned long timestamp_;  // NOLINT

  unsigned char* encodedBuffer_;
  unsigned char* packagedBuffer_;
  unsigned char* rtpBuffer_;

  unsigned char* encodedAudioBuffer_;
  unsigned char* packagedAudioBuffer_;

  MediaInfo mediaInfo;

  RTPDataReceiver* rtpReceiver_;

  AVCodec* aCoder;
  AVCodecContext* aCoderContext;

  VideoEncoder vCoder;

  RtpVP8Parser pars;

  bool initAudioCoder();

  bool initAudioPackager();
  bool initVideoPackager();

  int encodeAudio(unsigned char* inBuff, int nSamples, AVPacket* pkt);
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_MEDIA_MEDIAPROCESSOR_H_
