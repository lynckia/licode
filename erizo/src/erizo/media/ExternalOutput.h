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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace erizo{
  class WebRtcConnection;

  class ExternalOutput : public MediaSink, public RawDataReceiver {

    public:
      ExternalOutput (std::string outputUrl);
      virtual ~ExternalOutput();
      bool init();
	    int deliverAudioData(char* buf, int len);
	    int deliverVideoData(char* buf, int len);
      void receiveRawData(RawDataPacket& packet);
      void closeSink();


    private:
      OutputProcessor* op_;
      unsigned char* decodedBuffer_;
      char* sendVideoBuffer_;
      bool initContext();
      void encodeLoop();

      std::string url;
      bool running;
	    boost::mutex queueMutex_;
      boost::thread thread_, encodeThread_;
      std::queue<RawDataPacket> packetQueue_;
      AVStream        *video_st, *audio_st;
      
      AudioEncoder* audioCoder_;
      unsigned char* unpackagedBuffer_;
      unsigned char* unpackagedAudioBuffer_;
      int gotUnpackagedFrame_;
      int unpackagedSize_;
      int prevEstimatedFps_;
      int warmupfpsCount_;


      AVFormatContext *context_;
      AVOutputFormat *oformat_;
      AVCodec *videoCodec_, *audioCodec_; 
      AVCodecContext *videoCodecCtx_, *audioCodecCtx_;
      InputProcessor *in;
    

      int video_stream_index, bufflen;
      AVPacket avpacket;
      char *deliverMediaBuffer_;
      long initTime_;
  };
}
#endif /* EXTERNALOUTPUT_H_ */
