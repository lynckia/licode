#include "ExternalInput.h"
#include <cstdio>

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace erizo {
  ExternalInput::ExternalInput(const std::string& url): RTPDataReceiver() {

    printf("Constructor\n");


    AVFormatContext *_formatCtx;
    AVCodecContext  *_codecCtx;
    AVCodec         *_codec;
    AVFrame         *_frame;
    AVPacket        _packet;
    AVDictionary    *_optionsDict;


    AVFormatContext* context = avformat_alloc_context();
    int video_stream_index;

    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    //open rtsp
    printf("trying to open input\n");
    if(avformat_open_input(&context, url.c_str(),NULL,NULL) != 0){
      printf("fail when opening input\n");
    }
    if(avformat_find_stream_info(context,NULL) < 0){
      printf("fail when finding stream info\n");
    }

    VideoCodecInfo info;
    info.payloadType = 98;
    info.width = 640;
    info.height = 480;
    int bufflen = 640*480*3/2;
    decodedBuffer_ = (unsigned char*) malloc(bufflen);
    info.codec = VIDEO_CODEC_H264;

    inCodec_->initDecoder(info);


    MediaInfo om;
    om.proccessorType = RTP_ONLY;
    om.videoCodec.codec = VIDEO_CODEC_VP8;
    om.videoCodec.bitRate = 2000000;
    om.videoCodec.width = 640;
    om.videoCodec.height = 480;
    om.videoCodec.frameRate = 20;
    om.hasVideo = true;
    //	om.url = "file://tmp/test.mp4";

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

    AVPacket packet;
    av_init_packet(&packet);

    //open output file
    //    AVOutputFormat* fmt = av_guess_format(NULL,"test2.avi",NULL);
    //    AVFormatContext* oc = avformat_alloc_context();
    //    oc->oformat = fmt;
    //    avio_open2(&oc->pb, "test.avi", AVIO_FLAG_WRITE,NULL,NULL);

    AVStream* stream=NULL;
    int cnt = 0;
    int gotDecodedFrame = 0;
    //start reading packets from stream and write them to file
    av_read_play(context);//play RTSP
    while(av_read_frame(context,&packet)>=0){//read 100 frames
      if(packet.stream_index == video_stream_index){//packet is video               
        if(stream == NULL){//create stream in file
          //                stream = avformat_new_stream(oc,context->streams[video_stream_index]->codec->codec);
          //                avcodec_copy_context(stream->codec,context->streams[video_stream_index]->codec);
          //                stream->sample_aspect_ratio = context->streams[video_stream_index]->codec->sample_aspect_ratio;
          //                avformat_write_header(oc,NULL);
        }
        //        packet.stream_index = stream->id;
        //
        printf("eEad PAcket size: %u\n", packet.size);
        inCodec_->decodeVideo(packet.data, packet.size, decodedBuffer_, bufflen, &gotDecodedFrame);

        if (gotDecodedFrame){
          gotDecodedFrame=0;
        }
        //            av_write_frame(oc,&packet);


      }
      av_free_packet(&packet);
      av_init_packet(&packet);
    }
    av_read_pause(context);
    //    av_write_trailer(oc);
    //    avio_close(oc->pb);
    //    avformat_free_context(oc);
  }


	void ExternalInput::receiveRtpData(unsigned char* rtpdata, int len){

  }
  ExternalInput::~ExternalInput(){
    printf("Destructor\n");
  }
}
