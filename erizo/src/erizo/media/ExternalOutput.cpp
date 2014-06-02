#include "ExternalOutput.h"
#include "../WebRtcConnection.h"
#include "../rtputils.h"
#include <cstdio>

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>
#include "rtp/RtpHeader.h"

namespace erizo {
#define FIR_INTERVAL_MS 4000

DEFINE_LOGGER(ExternalOutput, "media.ExternalOutput");

ExternalOutput::ExternalOutput(const std::string& outputUrl)
{
    ELOG_DEBUG("Created ExternalOutput to %s", outputUrl.c_str());

    // TODO these should really only be called once per application run
    av_register_all();
    avcodec_register_all();


    context_ = avformat_alloc_context();
    if (context_==NULL){
        ELOG_ERROR("Error allocating memory for IO context");
    } else {

        outputUrl.copy(context_->filename, sizeof(context_->filename),0);

        context_->oformat = av_guess_format(NULL,  context_->filename, NULL);
        if (!context_->oformat){
            ELOG_ERROR("Error guessing format %s", context_->filename);
        } else {
            context_->oformat->video_codec = AV_CODEC_ID_VP8;
            context_->oformat->audio_codec = AV_CODEC_ID_NONE; // We'll figure this out once we start receiving data; it's either PCM or OPUS
        }
    }

    videoCodec_ = NULL;
    audioCodec_ = NULL;
    video_stream_ = NULL;
    audio_stream_ = NULL;
    prevEstimatedFps_ = 0;
    warmupfpsCount_ = 0;
    writeheadres_=-1;
    unpackagedBufferpart_ = unpackagedBuffer_;
    initTimeVideo_ = -1;
    initTimeAudio_ = -1;
    lastFullIntraFrameRequest_ = 0;
    sinkfbSource_ = this;
    fbSink_ = NULL;
    gotUnpackagedFrame_ = 0;
    unpackagedSize_ = 0;
    inputProcessor_ = new InputProcessor();
}

bool ExternalOutput::init(){
    MediaInfo m;
    m.hasVideo = false;
    m.hasAudio = false;
    inputProcessor_->init(m, this);
    thread_ = boost::thread(&ExternalOutput::sendLoop, this);
    recording_ = true;
    ELOG_DEBUG("Initialized successfully");
    return true;
}


ExternalOutput::~ExternalOutput(){
    ELOG_DEBUG("ExternalOutput Destructing");

    // Stop our thread so we can safely nuke libav stuff and close our
    // our file.
    recording_ = false;
    cond_.notify_one();
    thread_.join();

    delete inputProcessor_;
    inputProcessor_ = NULL;

    if (context_!=NULL && writeheadres_ >= 0)
        av_write_trailer(context_);

    if (video_stream_->codec != NULL){
        avcodec_close(video_stream_->codec);
    }

    if (audio_stream_->codec != NULL){
        avcodec_close(audio_stream_->codec);
    }

    if (context_ != NULL){
        avio_close(context_->pb);
        avformat_free_context(context_);
        context_ = NULL;
    }

    ELOG_DEBUG("ExternalOutput closed Successfully");
}

void ExternalOutput::receiveRawData(RawDataPacket& /*packet*/){
    return;
}


void ExternalOutput::writeAudioData(char* buf, int len){
    RTPHeader* head = reinterpret_cast<RTPHeader*>(buf);

    // Figure out our audio codec.
    if(context_->oformat->audio_codec == AV_CODEC_ID_NONE) {
        //We dont need any other payload at this time
        if(head->getPayloadType() == PCMU_8000_PT){
            context_->oformat->audio_codec = AV_CODEC_ID_PCM_MULAW;
        } else if (head->getPayloadType() == OPUS_48000_PT) {
            context_->oformat->audio_codec = AV_CODEC_ID_OPUS;
        }
    }

    if (videoCodec_ == NULL) {
        return;
    }

    int ret = inputProcessor_->unpackageAudio(reinterpret_cast<unsigned char*>(buf), len, unpackagedAudioBuffer_);
    if (ret <= 0)
        return;

        timeval time;
        gettimeofday(&time, NULL);
        unsigned long long millis = (time.tv_sec * 1000) + (time.tv_usec / 1000);
        if (millis -lastFullIntraFrameRequest_ >FIR_INTERVAL_MS){
            this->sendFirPacket();
            lastFullIntraFrameRequest_ = millis;
        }

    if (initTimeAudio_ == -1) {
        initTimeAudio_ = head->getTimestamp();
    }

//    ELOG_DEBUG("Writing audio frame %d, input timebase: %d/%d, target timebase: %d/%d", (int)(head->getTimestamp() - initTimeAudio_),
//               audio_stream_->codec->time_base.num, audio_stream_->codec->time_base.den,    // timebase we requested
//               audio_stream_->time_base.num, audio_stream_->time_base.den);                 // actual timebase

    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = unpackagedAudioBuffer_;
    avpkt.size = ret;
    avpkt.pts = (head->getTimestamp() - initTimeAudio_) / (audio_stream_->codec->time_base.den / audio_stream_->time_base.den);
    avpkt.stream_index = 1;
    av_write_frame(context_, &avpkt);
    av_free_packet(&avpkt);
}

void ExternalOutput::writeVideoData(char* buf, int len){
    RTPHeader* head = reinterpret_cast<RTPHeader*>(buf);
    if (head->getPayloadType() == RED_90000_PT) {
        int totalLength = head->getHeaderLength();
        int rtpHeaderLength = totalLength;
        redheader *redhead = (redheader*) (buf + totalLength);
        if (redhead->payloadtype == VP8_90000_PT) {
            while (redhead->follow) {
                totalLength += redhead->getLength() + 4; // RED header
                redhead = (redheader*) (buf + totalLength);
            }
            // Parse RED packet to VP8 packet.
            // Copy RTP header
            memcpy(deliverMediaBuffer_, buf, rtpHeaderLength);
            // Copy payload data
            memcpy(deliverMediaBuffer_ + totalLength, buf + totalLength + 1, len - totalLength - 1);
            // Copy payload type
            RTPHeader *mediahead = reinterpret_cast<RTPHeader*>(deliverMediaBuffer_);
            mediahead->setPayloadType(redhead->payloadtype);
            buf = reinterpret_cast<char*>(deliverMediaBuffer_);
            len = len - 1 - totalLength + rtpHeaderLength;
        }
    }

    int estimatedFps=0;
    int ret = inputProcessor_->unpackageVideo(reinterpret_cast<unsigned char*>(buf), len, unpackagedBufferpart_, &gotUnpackagedFrame_, &estimatedFps);
    if (ret < 0)
        return;

    if (videoCodec_ == NULL) {
        if ((estimatedFps!=0)&&((estimatedFps < prevEstimatedFps_*(1-0.2))||(estimatedFps > prevEstimatedFps_*(1+0.2)))){
            prevEstimatedFps_ = estimatedFps;
        }
        if (warmupfpsCount_++ > 20){
            if (prevEstimatedFps_==0){
                warmupfpsCount_ = 0;
                return;
            }
            if (!this->initContext()){
                return;
            }
        }
    }

    unpackagedSize_ += ret;
    unpackagedBufferpart_ += ret;

    if (gotUnpackagedFrame_ && videoCodec_ != NULL) {
        if (initTimeVideo_ == -1) {
            initTimeVideo_ = head->getTimestamp();
        }
        unpackagedBufferpart_ -= unpackagedSize_;

        long long timestampToWrite = (head->getTimestamp() - initTimeVideo_) / (90000 / video_stream_->time_base.den);  // All of our video offerings are using a 90khz clock.

//        ELOG_DEBUG("Writing video frame %d with timestamp %d, length %d, input timebase: %d/%d, target timebase: %d/%d", head->getSeqNumber(),
//                   (int)timestampToWrite, unpackagedSize_,
//                   video_stream_->codec->time_base.num, video_stream_->codec->time_base.den,    // timebase we requested
//                   video_stream_->time_base.num, video_stream_->time_base.den);                 // actual timebase

        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = unpackagedBufferpart_;
        avpkt.size = unpackagedSize_;
        avpkt.pts = timestampToWrite;
        avpkt.stream_index = 0;
        av_write_frame(context_, &avpkt);
        av_free_packet(&avpkt);
        gotUnpackagedFrame_ = 0;
        unpackagedSize_ = 0;
        unpackagedBufferpart_ = unpackagedBuffer_;
    }
}

int ExternalOutput::deliverAudioData_(char* buf, int len) {
    this->queueData(buf,len,AUDIO_PACKET);
    return 0;
}

int ExternalOutput::deliverVideoData_(char* buf, int len) {
    this->queueData(buf,len,VIDEO_PACKET);
    return 0;
}


bool ExternalOutput::initContext() {
    if (context_->oformat->video_codec != AV_CODEC_ID_NONE &&
            context_->oformat->audio_codec != AV_CODEC_ID_NONE &&
            videoCodec_ == NULL &&
            audioCodec_ == NULL) {
        videoCodec_ = avcodec_find_encoder(context_->oformat->video_codec);
        ELOG_DEBUG("Found Video Codec %s, initializing context with fps %d", videoCodec_->name, prevEstimatedFps_);
        if (videoCodec_==NULL){
            ELOG_ERROR("Could not find video codec");
            return false;
        }
        video_stream_ = avformat_new_stream (context_, videoCodec_);
        video_stream_->id = 0;
        video_stream_->codec->codec_id = context_->oformat->video_codec;
        video_stream_->codec->width = 640;
        video_stream_->codec->height = 480;
        video_stream_->codec->time_base = (AVRational){1,(int)prevEstimatedFps_};   // TODO this seems wrong.
        video_stream_->codec->pix_fmt = PIX_FMT_YUV420P;
        if (context_->oformat->flags & AVFMT_GLOBALHEADER){
            video_stream_->codec->flags|=CODEC_FLAG_GLOBAL_HEADER;
        }
        context_->oformat->flags |= AVFMT_VARIABLE_FPS;

        audioCodec_ = avcodec_find_encoder(context_->oformat->audio_codec);
        if (audioCodec_==NULL){
            ELOG_ERROR("Could not find audio codec");
            return false;
        }
        ELOG_DEBUG("Found Audio Codec %s", audioCodec_->name);
        audio_stream_ = avformat_new_stream (context_, audioCodec_);
        audio_stream_->id = 1;
        audio_stream_->codec->codec_id = context_->oformat->audio_codec;
        audio_stream_->codec->sample_rate = context_->oformat->audio_codec == AV_CODEC_ID_PCM_MULAW ? 8000 : 48000; // TODO is it always 48 khz for opus?
        audio_stream_->codec->time_base = (AVRational) { 1, audio_stream_->codec->sample_rate };
        audio_stream_->codec->channels = context_->oformat->audio_codec == AV_CODEC_ID_PCM_MULAW ? 1 : 2;   // TODO is it always two channels for opus?
        if (context_->oformat->flags & AVFMT_GLOBALHEADER){
            audio_stream_->codec->flags|=CODEC_FLAG_GLOBAL_HEADER;
        }

        context_->streams[0] = video_stream_;
        context_->streams[1] = audio_stream_;
        if (avio_open(&context_->pb, context_->filename, AVIO_FLAG_WRITE) < 0){
            ELOG_ERROR("Error opening output file");
            return false;
        }
        writeheadres_ = avformat_write_header(context_, NULL);
        if (writeheadres_<0){
            ELOG_ERROR("Error writing header");
            return false;
        }
        ELOG_DEBUG("AVFORMAT CONFIGURED");
    }

    return true;
}

void ExternalOutput::queueData(char* buffer, int length, packetType type){
    if (!recording_) {
        return;
    }
    rtcpheader *head = reinterpret_cast<rtcpheader*>(buffer);
    if (head->isRtcp()){
        return;
    }

    boost::mutex::scoped_lock lock(queueMutex_);
    if (type == VIDEO_PACKET){
        videoQueue_.pushPacket(buffer, length);
    }else{
        audioQueue_.pushPacket(buffer, length);
    }
    cond_.notify_one();
}

int ExternalOutput::sendFirPacket() {
    if (fbSink_ != NULL) {
        ELOG_DEBUG("ExternalOutput, sending Full Intra-frame Request");
        int pos = 0;
        uint8_t rtcpPacket[50];
        // add full intra request indicator
        uint8_t FMT = 4;
        rtcpPacket[pos++] = (uint8_t) 0x80 + FMT;
        rtcpPacket[pos++] = (uint8_t) 206;
        pos = 12;
        fbSink_->deliverFeedback((char*)rtcpPacket, pos);
        return pos;
    }

    return -1;
}

void ExternalOutput::sendLoop() {
    while (recording_) {
        boost::unique_lock<boost::mutex> lock(queueMutex_);
        while ((audioQueue_.getSize() < 30)&&(videoQueue_.getSize() < 30)) {
            cond_.wait(lock);
            if (!recording_) {
                lock.unlock();
                return;
            }
        }
        if (audioQueue_.getSize()){
            boost::shared_ptr<dataPacket> audioP = audioQueue_.popPacket();
            this->writeAudioData(audioP->data, audioP->length);
        }
        if (videoQueue_.getSize()) {
            boost::shared_ptr<dataPacket> videoP = videoQueue_.popPacket();
            this->writeVideoData(videoP->data, videoP->length);
        }

        lock.unlock();
    }
}
}

