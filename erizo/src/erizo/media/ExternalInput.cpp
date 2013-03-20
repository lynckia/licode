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
    //    info.payloadType = 98;
    info.width = 640;
    info.height = 480;
    bufflen = 640*480*3/2;
    decodedBuffer_ = (unsigned char*) malloc(bufflen);
    info.codec = VIDEO_CODEC_H264;
    inCodec_.initDecoder(info);

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


    printf("Success!\n");
    //search video stream
    for(int i =0;i<context->nb_streams;i++){
      if(context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        video_stream_index = i;
    }

    av_init_packet(&avpacket);

    //open output file
    //    AVOutputFormat* fmt = av_guess_format(NULL,"test2.avi",NULL);
    //    AVFormatContext* oc = avformat_alloc_context();
    //    oc->oformat = fmt;
    //    avio_open2(&oc->pb, "test.avi", AVIO_FLAG_WRITE,NULL,NULL);

    AVStream* stream=NULL;
    int cnt = 0;

    thread_ = boost::thread(&ExternalInput::receiveLoop, this);
    running = true;
    encodeThread_ = boost::thread(&ExternalInput::encodeLoop, this);
    //start reading packets from stream and write them to file

    //    av_write_trailer(oc);
    //    avio_close(oc->pb);
    //    avformat_free_context(oc);

    return true;
  }

  void ExternalInput::closeSource() {
  }

  int ExternalInput::sendFirPacket() {
    return 0;
  }

  ExternalInput::~ExternalInput(){
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
        /*
           if(stream == NULL){//create stream in file
        //                stream = avformat_new_stream(oc,context->streams[video_stream_index]->codec->codec);
        //                avcodec_copy_context(stream->codec,context->streams[video_stream_index]->codec);
        //                stream->sample_aspect_ratio = context->streams[video_stream_index]->codec->sample_aspect_ratio;
        //                avformat_write_header(oc,NULL);
        }
        */
        //        packet.stream_index = stream->id;
        //
        //        printf("Read PAcket size: %u\n", avpacket.size);
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
        //            av_write_frame(oc,&packet);


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

