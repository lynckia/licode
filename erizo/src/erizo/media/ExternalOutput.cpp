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
  }

  bool ExternalOutput::init(){
    context = avformat_alloc_context();
    av_register_all();
    avcodec_register_all();
/*
    avformat_alloc_output_context2(&context, NULL, NULL,url);
    if (!context) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", outputURL);
    }
    if (!oc) {
        return 1;
    }
    fmt = oc->oformat;
    video_st = NULL;
    audio_st = NULL;
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        video_st = add_stream(oc, &video_codec, fmt->video_codec);
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        audio_st = add_stream(oc, &audio_codec, fmt->audio_codec);
    }
    */
    return true;
  }


  ExternalOutput::~ExternalOutput(){
    this->closeSink();
    printf("Destructor\n");
  }

  void ExternalOutput::closeSink() {

    return;
  }


	int ExternalOutput::deliverAudioData(char* buf, int len){
    return 0;
  }
	int ExternalOutput::deliverVideoData(char* buf, int len){
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

