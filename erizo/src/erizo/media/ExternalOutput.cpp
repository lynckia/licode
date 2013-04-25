#include "ExternalOutput.h"
#include "../WebRtcConnection.h"
#include "../RTPSink.h"
#include <cstdio>

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>

namespace erizo {
  ExternalOutput::ExternalOutput(std::string outputUrl){
    url = outputUrl;
    sinkfbSource_=NULL;

  }

  bool ExternalOutput::init(){
   av_register_all();
   avcodec_register_all();
   context_ = avformat_alloc_context();
   oformat_ = av_guess_format(NULL,  url.c_str(), NULL);
   if (!oformat_){
    printf("Error opening output file %s", url.c_str());
    return false;
   }
   context_->oformat = oformat_;
//   av_strlcpy(context->filename, url.c_str(), sizeof(context->filename));
    url.copy(context_->filename, sizeof(context_->filename),0);
    video_st = NULL;
    audio_st = NULL;
    if (oformat_->video_codec != AV_CODEC_ID_NONE) {
      printf("AAAAAOOOOO\n\n");
          videoCodec_ = avcodec_find_encoder(oformat_->video_codec);
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
    }

    printf("1\n");
    in = new InputProcessor();
    printf("2\n");
    MediaInfo m;
//    m.processorType = RTP_ONLY;
	  m.hasVideo = true;
  	m.videoCodec.width = 640;
  	m.videoCodec.height = 480;
  	m.hasAudio = false;
    printf("init ip\n");
  	in->init(m, this);
    return true;
  }


  ExternalOutput::~ExternalOutput(){
    this->closeSink();
    printf("Destructor\n");
  }

  void ExternalOutput::closeSink() {

    return;
  }

  void ExternalOutput::receiveRawData(RawDataPacket& packet){
    printf("rawdata received\n");
    return;
  }


	int ExternalOutput::deliverAudioData(char* buf, int len){
    return 0;
  }
	int ExternalOutput::deliverVideoData(char* buf, int len){
    if (in!=NULL){
      in->deliverVideoData(buf,len);
    }
    return 0;
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

