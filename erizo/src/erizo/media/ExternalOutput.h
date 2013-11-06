#ifndef EXTERNALOUTPUT_H_
#define EXTERNALOUTPUT_H_

#include <string> 
#include <map>
#include <queue>
#include "../MediaDefinitions.h"
#include "codecs/VideoCodec.h"
#include "codecs/AudioCodec.h"
#include "MediaProcessor.h"
#include "boost/thread.hpp"
#include "logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace erizo{
  class WebRtcConnection;

  class ExternalOutput : public MediaSink, public RawDataReceiver, public FeedbackSource {
      DECLARE_LOGGER();
    public:
      ExternalOutput (std::string outputUrl);
      virtual ~ExternalOutput();
      bool init();
	    int deliverAudioData(char* buf, int len);
	    int deliverVideoData(char* buf, int len);
      void receiveRawData(RawDataPacket& packet);

    private:
      OutputProcessor* op_;
      unsigned char* decodedBuffer_;
      char* sendVideoBuffer_;
      bool initContext();
      int sendFirPacket();
      void encodeLoop();

      std::string url;
      bool running;
	    boost::mutex queueMutex_;
      boost::thread thread_, encodeThread_;
      std::queue<RawDataPacket> packetQueue_;
      AVStream        *video_st, *audio_st;
      
      AudioEncoder* audioCoder_;
      unsigned char* unpackagedBuffer_,*unpackagedBufferpart_;
      unsigned char* unpackagedAudioBuffer_;
      int gotUnpackagedFrame_;
      int unpackagedSize_;
      int prevEstimatedFps_;
      int warmupfpsCount_;
      int sequenceNumberFIR_;
      unsigned long lastTime_;

      int video_stream_index, bufflen, aviores_, writeheadres_;


      AVFormatContext *context_;
      AVOutputFormat *oformat_;
      AVCodec *videoCodec_, *audioCodec_; 
      AVCodecContext *videoCodecCtx_, *audioCodecCtx_;
      InputProcessor *in;


      AVPacket avpacket;
      char *deliverMediaBuffer_;
      unsigned long initTime_;
  };
}
#endif /* EXTERNALOUTPUT_H_ */
