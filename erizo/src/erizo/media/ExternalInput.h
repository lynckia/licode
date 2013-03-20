#ifndef EXTERNALINPUT_H_
#define EXTERNALINPUT_H_

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

  class ExternalInput : public MediaSource, public RTPDataReceiver {

    public:
      ExternalInput (std::string inputUrl);
      virtual ~ExternalInput();
      bool init();
      void receiveRtpData(unsigned char*rtpdata, int len);
      int sendFirPacket();
      void closeSource();


    private:
      OutputProcessor* op_;
      VideoDecoder inCodec_;
      unsigned char* decodedBuffer_;
      char* sendVideoBuffer_;
      void receiveLoop();
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
      AVPacket        _packet;
      AVDictionary    *_optionsDict;
      AVFormatContext* context;
      int video_stream_index, bufflen;
      AVPacket avpacket;
  };
}
#endif /* EXTERNALINPUT_H_ */
