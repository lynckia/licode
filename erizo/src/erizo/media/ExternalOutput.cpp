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
    unpackagedBuffer_ = NULL;
    audioSinkSSRC_ = 0;
    videoSinkSSRC_ = 0; 

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
   url.copy(context_->filename, sizeof(context_->filename),0);
   video_st = NULL;
   audio_st = NULL;
   if (oformat_->video_codec != AV_CODEC_ID_NONE) {
     videoCodec_ = avcodec_find_encoder(oformat_->video_codec);
     printf("Found Codec %s\n", videoCodec_->name);
     if (videoCodec_->id == AV_CODEC_ID_VP8){

       printf("VP88888888888!!!! %d, context_id %d\n", videoCodec_->id, context_->video_codec_id);
     }
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
     videoCodecCtx_->time_base = (AVRational){1,20};
     videoCodecCtx_->pix_fmt = PIX_FMT_YUV420P;
     if (oformat_->flags & AVFMT_GLOBALHEADER){
       videoCodecCtx_->flags|=CODEC_FLAG_GLOBAL_HEADER;
     }
     avcodec_open2(videoCodecCtx_, videoCodec_, NULL);
     context_->streams[0] = video_st;
     avio_open(&context_->pb, url.c_str(), AVIO_FLAG_WRITE);
     avformat_write_header(context_, NULL);
     printf("AVFORMAT CONFIGURED\n");
   }
   in = new InputProcessor();
   MediaInfo m;
//    m.processorType = RTP_ONLY;
	 m.hasVideo = true;
   m.videoCodec.width = 640;
   m.videoCodec.height = 480;
   m.hasAudio = false;
   printf("********************* INIT IP\n");
   unpackagedBuffer_ = (unsigned char*)malloc (15000);
   gotUnpackagedFrame_ = 0;
   unpackagedSize_ = 0;
   in->init(m, this);
   return true;
  }


  ExternalOutput::~ExternalOutput(){
    this->closeSink();
    printf("Destructor\n");
  }

  void ExternalOutput::closeSink() {
    if (unpackagedBuffer_ !=NULL){
      free(unpackagedBuffer_);
    }
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
        int ret = in->unpackageVideo(reinterpret_cast<unsigned char*>(buf), len,
            unpackagedBuffer_, &gotUnpackagedFrame_);
        if (ret < 0)
          return 0;
        unpackagedSize_ += ret;
        unpackagedBuffer_ += ret;
        if (gotUnpackagedFrame_) {
          unpackagedBuffer_ -= unpackagedSize_;
          printf("Tengo un frame desempaquetado!! Size = %d\n",
              unpackagedSize_);
          printf("1\n");
          AVPacket avpkt;
          printf("2\n");
          av_init_packet(&avpkt);
          avpkt.data = unpackagedBuffer_;
          avpkt.size = unpackagedSize_;
          avpkt.stream_index = 0;
          printf("3\n");
          av_write_frame(context_, &avpkt);
          av_free_packet(&avpkt);
          printf("4\n");
          gotUnpackagedFrame_ = 0;
          unpackagedSize_ = 0;
      }
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

