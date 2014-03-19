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
    if (needTranscoding_)
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

    MediaInfo om;
    AVStream *st, *audio_st;
    
    
    int streamNo = av_find_best_stream(context_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (streamNo < 0){
      ELOG_WARN("No Video stream found");
      //return streamNo;
    }else{
      om.hasVideo = true;
      video_stream_index_ = streamNo;
      st = context_->streams[streamNo]; 
    }

    int audioStreamNo = av_find_best_stream(context_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audioStreamNo < 0){
      ELOG_WARN("No Audio stream found");
      //return streamNo;
    }else{
      om.hasAudio = true;
      audio_stream_index_ = audioStreamNo;
      audio_st = context_->streams[audio_stream_index_];
      ELOG_DEBUG(" HAS AUDIO, audio time base = %d / %d ", audio_st->time_base.num, audio_st->time_base.den);
      audio_time_base_ = st->time_base.den;
    }

    
    if (st->codec->codec_id==AV_CODEC_ID_VP8){
      ELOG_DEBUG("No need for video transcoding, already VP8");      
      video_time_base_ = st->time_base.den;
      needTranscoding_=false;
      decodedBuffer_.reset((unsigned char*) malloc(100000));
      MediaInfo om;
      om.processorType = PACKAGE_ONLY;
      op_.reset(new OutputProcessor());
      op_->init(om,this);
    }else{
      needTranscoding_=true;
      inCodec_.initDecoder(st->codec);


      bufflen_ = st->codec->width*st->codec->height*3/2;
      decodedBuffer_.reset((unsigned char*) malloc(bufflen_));


      om.processorType = RTP_ONLY;
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
    }

    av_init_packet(&avpacket_);

//    AVStream* stream=NULL;

    ELOG_DEBUG("Initializing external input for codec %s", st->codec->codec_name);
    thread_ = boost::thread(&ExternalInput::receiveLoop, this);
    running_ = true;
    if (needTranscoding_)
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

    /** RATE EMU
             int64_t pts = av_rescale(ist->pts, 1000000, AV_TIME_BASE);
>             int64_t now = av_gettime() - ist->start;
>             if (pts > now)
>                 usleep(pts - now);
     */
    av_read_play(context_);//play RTSP
    int gotDecodedFrame = 0;
    int length;
    float sleepTimeSecs;
    startTime_ = av_gettime();
    while(av_read_frame(context_,&avpacket_)>=0&& running_==true){//read 100 frames
      if (needTranscoding_){
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
      }else{
        if(avpacket_.stream_index == video_stream_index_){//packet is video               
          int64_t pts = av_rescale(lastPts_, 1000000, (long int)video_time_base_);
          int64_t now = av_gettime() - startTime_;
          if (pts > now)
            av_usleep(pts - now);
          lastPts_ = avpacket_.pts;
          op_->packageVideo(avpacket_.data, avpacket_.size, decodedBuffer_.get(), avpacket_.pts);
        }else if(avpacket_.stream_index == audio_stream_index_){//packet is audio
          
          int64_t pts = av_rescale(lastAudioPts_, 1000000, (long int)audio_time_base_);
          int64_t now = av_gettime() - startTime_;
          if (pts > now)
            av_usleep(pts - now);
          lastAudioPts_ = avpacket_.pts;
          
          length = op_->packageAudio(avpacket_.data, avpacket_.size, decodedBuffer_.get());
          if (length>0){
            audioSink_->deliverAudioData(reinterpret_cast<char*>(decodedBuffer_.get()),length);
          }

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

