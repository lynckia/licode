#include "ExternalOutput.h"
#include "../WebRtcConnection.h"
#include "../RTPSink.h"
#include <cstdio>

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>

namespace erizo {
  ExternalOutput::ExternalOutput(std::string outputUrl){
    printf("Created ExternalOutput to %s\n", outputUrl.c_str());
    url = outputUrl;
    sinkfbSource_=NULL;
    unpackagedBuffer_ = NULL;
    audioSinkSSRC_ = 0;
    videoSinkSSRC_ = 0; 
    videoCodec_ = NULL;;
    prevEstimatedFps_ = 0;
    warmupfpsCount_ = 0;
  }

  bool ExternalOutput::init(){
    printf("externalouput init begin\n");
    av_register_all();
    avcodec_register_all();
    context_ = avformat_alloc_context();
    oformat_ = av_guess_format(NULL,  url.c_str(), NULL);
    if (!oformat_){
      printf("Error opening output file %s\n", url.c_str());
      return false;
    }
    context_->oformat = oformat_;
    url.copy(context_->filename, sizeof(context_->filename),0);
    video_st = NULL;
    audio_st = NULL;
    in = new InputProcessor();
    MediaInfo m;
    //    m.processorType = RTP_ONLY;
    m.hasVideo = false;
    m.hasAudio = false;
    if (m.hasAudio) {
      m.audioCodec.sampleRate = 8000;
      m.audioCodec.bitRate = 64000;
    }
    unpackagedBuffer_ = (unsigned char*)malloc (15000);
    gotUnpackagedFrame_ = 0;
    unpackagedSize_ = 0;
    in->init(m, this);
    printf("externalouput init end\n");
    return true;
  }


  ExternalOutput::~ExternalOutput(){
    printf("Destructor\n");
    this->closeSink();
  }

  void ExternalOutput::closeSink() {
    printf("ExternalOutput::closeSink\n");
    if (context_!=NULL){
      av_write_trailer(context_);
      avformat_free_context(context_);
    }
    if (unpackagedBuffer_ !=NULL){
      free(unpackagedBuffer_);
    }
    return;
  }

  void ExternalOutput::receiveRawData(RawDataPacket& packet){
//    printf("rawdata received\n");
    return;
  }


  int ExternalOutput::deliverAudioData(char* buf, int len){
//    printf("Deliver audio to EXTERNAL\n");
    return 0;
  }

  int ExternalOutput::deliverVideoData(char* buf, int len){
    if (in!=NULL){
      int estimatedFps=0;
      int ret = in->unpackageVideo(reinterpret_cast<unsigned char*>(buf), len,
          unpackagedBuffer_, &gotUnpackagedFrame_, &estimatedFps);
      //printf("Estimated FPS %d, previous %d\n", estimatedFps, prevEstimatedFps_);
      if (videoCodec_ == NULL) {
        if (estimatedFps!=0&&(estimatedFps < prevEstimatedFps_*(1-0.2))||(estimatedFps > prevEstimatedFps_*(1+0.2))){
          //printf("OUT OF THRESHOLD changing context\n");
          prevEstimatedFps_ = estimatedFps;          
        }
        if (warmupfpsCount_++ >10){
          this->initContext();
        }
      }
      if (ret < 0)
        return 0;
      unpackagedSize_ += ret;
      unpackagedBuffer_ += ret;
      if (gotUnpackagedFrame_ && videoCodec_!=NULL) {
        unpackagedBuffer_ -= unpackagedSize_;
        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = unpackagedBuffer_;
        avpkt.size = unpackagedSize_;
        avpkt.stream_index = 0;
        av_write_frame(context_, &avpkt);
        av_free_packet(&avpkt);
        gotUnpackagedFrame_ = 0;
        unpackagedSize_ = 0;
      }
    }
    return 0;
  }

  bool ExternalOutput::initContext() {
    printf("Init Context\n");
    if (oformat_->video_codec != AV_CODEC_ID_NONE && videoCodec_ == NULL) {
      videoCodec_ = avcodec_find_encoder(oformat_->video_codec);
      printf("Found Codec %s\n", videoCodec_->name);
      printf("Initing context with fps: %d\n", (int)prevEstimatedFps_);
      if (videoCodec_==NULL){
        printf("Could not find codec\n");
        return false;
      }
      video_st = avformat_new_stream (context_, videoCodec_);
      video_st->id = 0;
      videoCodecCtx_ = video_st->codec;
      videoCodecCtx_->codec_id = oformat_->video_codec;
      videoCodecCtx_->width = 640;
      videoCodecCtx_->height = 480;
      videoCodecCtx_->time_base = (AVRational){1,(int)prevEstimatedFps_};
      videoCodecCtx_->pix_fmt = PIX_FMT_YUV420P;
      if (oformat_->flags & AVFMT_GLOBALHEADER){
        videoCodecCtx_->flags|=CODEC_FLAG_GLOBAL_HEADER;
      }
      oformat_->flags |= AVFMT_VARIABLE_FPS;
      context_->streams[0] = video_st;
      avio_open(&context_->pb, url.c_str(), AVIO_FLAG_WRITE);
      avformat_write_header(context_, NULL);
      printf("AVFORMAT CONFIGURED\n");
    }

  }

  void ExternalOutput::encodeLoop() {
    while (running == true) {
      queueMutex_.lock();
      if (packetQueue_.size() > 0) {
        op_->receiveRawData(packetQueue_.front());
        packetQueue_.pop();

        queueMutex_.unlock();
      } else {
        queueMutex_.unlock();
        usleep(1000);
      }
    }
  }

}

