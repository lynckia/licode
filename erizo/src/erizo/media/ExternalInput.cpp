#include "ExternalInput.h"
#include "../WebRtcConnection.h"
#include "../RTPSink.h"
#include <cstdio>

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>

namespace erizo {
  
  ExternalInput::ExternalInput(const std::string& inputUrl){
    sourcefbSink_=NULL;
    context_ = NULL;
    sendVideoBuffer_=NULL;
    running_ = false;
    url_ = inputUrl;
  }

  ExternalInput::~ExternalInput(){
    printf("Destructor EI\n");
    this->closeSource();
  }

  int ExternalInput::init(){
    context_ = avformat_alloc_context();
    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    //open rtsp
    printf("trying to open input\n");
    int res = avformat_open_input(&context_, url_.c_str(),NULL,NULL);
    char errbuff[500];
    printf ("RES %d\n", res);
    if(res != 0){
      av_strerror(res, (char*)(&errbuff), 500);
      printf("fail when opening input %s\n", errbuff);
      return false;
    }
    res = avformat_find_stream_info(context_,NULL);
    if(res!=0){
      av_strerror(res, (char*)(&errbuff), 500);
      printf("fail when finding stream info %s\n", errbuff);
      return false;
    }

    VideoCodecInfo info;

    int streamNo = av_find_best_stream(context_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamNo < 0){
      printf("No video stream?\n");
      return false;
    }
    sendVideoBuffer_ = (char*) malloc(2000);

    video_stream_index_ = streamNo;
    AVStream* st = context_->streams[streamNo];    
    inCodec_.initDecoder(st->codec);

    bufflen_ = st->codec->width*st->codec->height*3/2;
    decodedBuffer_ = (unsigned char*) malloc(bufflen_);

    MediaInfo om;
    om.proccessorType = RTP_ONLY;
    om.videoCodec.codec = VIDEO_CODEC_VP8;
    om.videoCodec.bitRate = 1000000;
    om.videoCodec.width = 640;
    om.videoCodec.height = 480;
    om.videoCodec.frameRate = 20;
    om.hasVideo = true;

    om.hasAudio = false;
    if (om.hasAudio) {
      om.audioCodec.sampleRate = 8000;
      om.audioCodec.bitRate = 64000;
    }

    op_ = new OutputProcessor();
    op_->init(om, this);


    printf("Success initializing external input for codec %s\n", st->codec->codec_name);
    av_init_packet(&avpacket_);

    AVStream* stream=NULL;
    int cnt = 0;

    thread_ = boost::thread(&ExternalInput::receiveLoop, this);
    running_ = true;
    encodeThread_ = boost::thread(&ExternalInput::encodeLoop, this);
    return true;
  }

  void ExternalInput::closeSource() {
    running_ = false;
    encodeThread_.join();
    thread_.join();
    av_free_packet(&avpacket_);
    if (context_!=NULL)
      avformat_free_context(context_);
    if (sendVideoBuffer_!=NULL)
      free(sendVideoBuffer_);
    if(decodedBuffer_!=NULL)
      free(decodedBuffer_);
  }

  int ExternalInput::sendFirPacket() {
    return 0;
  }


  void ExternalInput::receiveRtpData(unsigned char*rtpdata, int len) {
    if (videoSink_!=NULL){
      memcpy(sendVideoBuffer_, rtpdata, len);
      videoSink_->deliverVideoData(sendVideoBuffer_, len);
    }

  }

  void ExternalInput::receiveLoop(){
    av_read_play(context_);//play RTSP
    int gotDecodedFrame = 0;
    while(av_read_frame(context_,&avpacket_)>=0&& running_==true){//read 100 frames
      if(avpacket_.stream_index == video_stream_index_){//packet is video               
        //        packet.stream_index = stream->id;
        inCodec_.decodeVideo(avpacket_.data, avpacket_.size, decodedBuffer_, bufflen_, &gotDecodedFrame);
        RawDataPacket packetR;
        if (gotDecodedFrame){
          packetR.data = decodedBuffer_;
          packetR.length = bufflen_;
          packetR.type = VIDEO;
          queueMutex_.lock();
          packetQueue_.push(packetR);
          queueMutex_.unlock();
          gotDecodedFrame=0;
        }
      }
      av_free_packet(&avpacket_);
      av_init_packet(&avpacket_);
    }
    running_=false;
    av_read_pause(context_);
  }

  void ExternalInput::encodeLoop() {
    while (running_ == true) {
      queueMutex_.lock();
      if (packetQueue_.size() > 0) {
        op_->receiveRawData(packetQueue_.front());
        packetQueue_.pop();

        //      printf("Queue Size! %d\n", packetQueue_.size());
        queueMutex_.unlock();
      } else {
        queueMutex_.unlock();
        usleep(1000);
      }
    }
  }

}

