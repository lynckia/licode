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
  class RTPSink;

  class ExternalInput : public MediaReceiver, public RTPDataReceiver {

    public:

      WebRtcConnection *publisher;
      std::map<std::string, WebRtcConnection*> subscribers;

      ExternalInput ();
      virtual ~ExternalInput();
      void init();
      /**
       * Sets the Publisher
       * @param webRtcConn The WebRtcConnection of the Publisher
       */
      void setPublisher(WebRtcConnection* webRtcConn);
      /**
       * Sets the subscriber
       * @param webRtcConn The WebRtcConnection of the subscriber
       * @param peerId An unique Id for the subscriber
       */
      void addSubscriber(WebRtcConnection* webRtcConn, const std::string& peerId);
      /**
       * Eliminates the subscriber given its peer id
       * @param peerId the peerId
       */
      void removeSubscriber(const std::string& peerId);

      int receiveAudioData(char* buf, int len);
      int receiveVideoData(char* buf, int len);
      void receiveRtpData(unsigned char*rtpdata, int len);
      void closeAll();


    private:
      OutputProcessor* op_;
      VideoDecoder inCodec_;
      unsigned char* decodedBuffer_;
      char* sendVideoBuffer_;
      void receiveLoop();
      void encodeLoop();

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

      RTPSink* sink_;

  };
}
#endif /* EXTERNALINPUT_H_ */
