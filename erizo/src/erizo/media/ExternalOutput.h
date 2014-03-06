#ifndef EXTERNALOUTPUT_H_
#define EXTERNALOUTPUT_H_

#include <string> 
#include <map>
#include <queue>
#include "../MediaDefinitions.h"
#include "rtp/RtpPacketQueue.h"
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
#define UNPACKAGE_BUFFER_SIZE 200000
  class WebRtcConnection;

  class ExternalOutput : public MediaSink, public RawDataReceiver, public FeedbackSource {
      DECLARE_LOGGER();
    public:
      ExternalOutput(const std::string& outputUrl);
      virtual ~ExternalOutput();
      bool init();
	    int deliverAudioData(char* buf, int len);
	    int deliverVideoData(char* buf, int len);
      void receiveRawData(RawDataPacket& packet);

    private:
      OutputProcessor* op_;
      RtpPacketQueue audioQueue_, videoQueue_;
      unsigned char* decodedBuffer_;
      char* sendVideoBuffer_;
      

      std::string url;
      volatile bool sending_;
	    boost::mutex queueMutex_;
      boost::thread thread_;
    	boost::condition_variable cond_;
      std::queue<dataPacket> packetQueue_;
      AVStream        *video_st, *audio_st;
      
      AudioEncoder* audioCoder_;
      int gotUnpackagedFrame_;
      int unpackagedSize_;
      int prevEstimatedFps_;
      int warmupfpsCount_;
      int sequenceNumberFIR_;
      unsigned long long lastTime_;

      int video_stream_index, bufflen, aviores_, writeheadres_;

      AVFormatContext *context_;
      AVOutputFormat *oformat_;
      AVCodec *videoCodec_, *audioCodec_; 
      AVCodecContext *videoCodecCtx_, *audioCodecCtx_;
      InputProcessor *in;

      AVPacket avpacket;
      unsigned char* unpackagedBufferpart_;
      unsigned char deliverMediaBuffer_[3000];
      unsigned char unpackagedBuffer_[UNPACKAGE_BUFFER_SIZE];
      unsigned char unpackagedAudioBuffer_[UNPACKAGE_BUFFER_SIZE/10];
      unsigned long long initTime_;
      
      
      bool initContext();
      int sendFirPacket();
      void queueData(char* buffer, int length, packetType type);
      void sendLoop();
	    int writeAudioData(char* buf, int len);
	    int writeVideoData(char* buf, int len);
  };
}
#endif /* EXTERNALOUTPUT_H_ */
