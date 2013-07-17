#ifndef EXTERNALOUTPUT_H_
#define EXTERNALOUTPUT_H_

#include <string> 
#include <map>
#include <queue>
#include "../MediaDefinitions.h"
#include "codecs/VideoCodec.h"
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
      VideoDecoder inCodec_;
      unsigned char* decodedBuffer_;
      char* sendVideoBuffer_;
      void encodeLoop();

      std::string url;
      bool running;
	    boost::mutex queueMutex_;
      boost::thread thread_, encodeThread_;
      std::queue<RawDataPacket> packetQueue_;
      AVFormatContext *_formatCtx;
      AVCodecContext  *_codecCtx;
      AVCodec         *_codec;
      AVFrame         *_frame;
      AVStream        *video_st, *audio_st;
      AVPacket        _packet;
      AVDictionary    *_optionsDict;

      unsigned char* unpackagedBuffer_;
      int gotUnpackagedFrame_;
      int unpackagedSize_;


      AVFormatContext *context_;
      AVOutputFormat *oformat_;
      AVCodec *videoCodec_; 
      AVCodecContext *videoCodecCtx_;
      InputProcessor *in;
    

      int video_stream_index, bufflen;
      AVPacket avpacket;
  };
}
#endif /* EXTERNALOUTPUT_H_ */
