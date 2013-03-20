#include "ExternalInput.h"
#include "../WebRtcConnection.h"
#include "../RTPSink.h"
#include <cstdio>

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>

namespace erizo {
  ExternalInput::ExternalInput(std::string inputUrl){

    //    printf("Constructor URL: %s\n", inputUrl.c_str());
    sourcefbSink_=NULL;
    url = inputUrl;
    
  }

  bool ExternalInput::init(){
    context = avformat_alloc_context();
    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    //open rtsp
    printf("trying to open input\n");
    if(avformat_open_input(&context, url.c_str(),NULL,NULL) != 0){
      printf("fail when opening input\n");
      return false;
    }
    if(avformat_find_stream_info(context,NULL) < 0){
      printf("fail when finding stream info\n");
      return false;
    }

    sendVideoBuffer_ = (char*) malloc(2000);
    VideoCodecInfo info;

    int ret;
    ret = av_find_best_stream(context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0){
      printf("No video stream?\n");
      return false;
    }
    video_stream_index = ret;

    AVStream* st = context->streams[ret];    
    inCodec_.initDecoder(st->codec);

    bufflen = st->codec->width*st->codec->height*3/2;
    decodedBuffer_ = (unsigned char*) malloc(bufflen);

    MediaInfo om;
    om.proccessorType = RTP_ONLY;
    om.videoCodec.codec = VIDEO_CODEC_VP8;
    om.videoCodec.bitRate = 100000;
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


    printf("Success initializing external input\n");
    av_init_packet(&avpacket);

    AVStream* stream=NULL;
    int cnt = 0;

    thread_ = boost::thread(&ExternalInput::receiveLoop, this);
    running = true;
    encodeThread_ = boost::thread(&ExternalInput::encodeLoop, this);
    return true;
  }

  void ExternalInput::closeSource() {
    running = false;
  }

  int ExternalInput::sendFirPacket() {
    return 0;
  }

  ExternalInput::~ExternalInput(){
    this->closeSource();
    printf("Destructor\n");
  }

  void ExternalInput::receiveRtpData(unsigned char*rtpdata, int len) {
    if (videoSink_!=NULL){
      memcpy(sendVideoBuffer_, rtpdata, len);
      videoSink_->deliverVideoData(sendVideoBuffer_, len);
    }

  }

  void ExternalInput::receiveLoop(){

    av_read_play(context);//play RTSP
    int gotDecodedFrame = 0;
    while(av_read_frame(context,&avpacket)>=0){//read 100 frames
      if(avpacket.stream_index == video_stream_index){//packet is video               
        //        packet.stream_index = stream->id;
        inCodec_.decodeVideo(avpacket.data, avpacket.size, decodedBuffer_, bufflen, &gotDecodedFrame);
        RawDataPacket packetR;
        if (gotDecodedFrame){
          packetR.data = decodedBuffer_;
          packetR.length = bufflen;
          packetR.type = VIDEO;
          queueMutex_.lock();
          packetQueue_.push(packetR);
          queueMutex_.unlock();
          gotDecodedFrame=0;
        }
      }
      av_free_packet(&avpacket);
      av_init_packet(&avpacket);
    }
    running=false;
    av_read_pause(context);
  }

  void ExternalInput::encodeLoop() {
    while (running == true) {
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

