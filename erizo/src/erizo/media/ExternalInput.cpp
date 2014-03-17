#include "ExternalInput.h"
#include "../WebRtcConnection.h"
#include <cstdio>

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>

namespace erizo {
  DEFINE_LOGGER(ExternalInput, "media.ExternalInput");
  ExternalInput::ExternalInput(const std::string& inputUrl){
    sourcefbSink_=NULL;
    context_ = NULL;
    running_ = false;
    url_ = inputUrl;
  }

  ExternalInput::~ExternalInput(){
    ELOG_DEBUG("Destructor ExternalInput %s" , url_.c_str());
    ELOG_DEBUG("Closing ExternalInput");
    running_ = false;
    thread_.join();
    encodeThread_.join();
    av_free_packet(&avpacket_);
    if (context_!=NULL)
      avformat_free_context(context_);
    ELOG_DEBUG("ExternalInput closed");
  }

  int ExternalInput::init(){
    context_ = avformat_alloc_context();
    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    //open rtsp
    ELOG_DEBUG("Trying to open input from url %s", url_.c_str());
    int res = avformat_open_input(&context_, url_.c_str(),NULL,NULL);
    char errbuff[500];
    printf ("RES %d\n", res);
    if(res != 0){
      av_strerror(res, (char*)(&errbuff), 500);
      ELOG_ERROR("Error opening input %s", errbuff);
      return res;
    }
    res = avformat_find_stream_info(context_,NULL);
    if(res<0){
      av_strerror(res, (char*)(&errbuff), 500);
      ELOG_ERROR("Error finding stream info %s", errbuff);
      return res;
    }

    //VideoCodecInfo info;

    int streamNo = av_find_best_stream(context_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamNo < 0){
      ELOG_ERROR("No stream found");
      return streamNo;
    }

    video_stream_index_ = streamNo;
    AVStream* st = context_->streams[streamNo];    
    inCodec_.initDecoder(st->codec);


    bufflen_ = st->codec->width*st->codec->height*3/2;
    decodedBuffer_.reset((unsigned char*) malloc(bufflen_));


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

    op_.reset(new OutputProcessor());
    op_->init(om, this);


    ELOG_DEBUG("Success initializing external input for codec %s", st->codec->codec_name);
    av_init_packet(&avpacket_);

//    AVStream* stream=NULL;

    thread_ = boost::thread(&ExternalInput::receiveLoop, this);
    running_ = true;
    encodeThread_ = boost::thread(&ExternalInput::encodeLoop, this);
    return true;
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
        inCodec_.decodeVideo(avpacket_.data, avpacket_.size, decodedBuffer_.get(), bufflen_, &gotDecodedFrame);
        RawDataPacket packetR;
        if (gotDecodedFrame){
          packetR.data = decodedBuffer_.get();
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
        queueMutex_.unlock();
      } else {
        queueMutex_.unlock();
        usleep(10000);
      }
    }
  }

}

