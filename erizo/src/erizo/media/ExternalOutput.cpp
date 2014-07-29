#include <sys/time.h>

#include "ExternalOutput.h"
#include "../WebRtcConnection.h"
#include "../rtp/RtpHeaders.h"

namespace erizo {
#define FIR_INTERVAL_MS 4000

  DEFINE_LOGGER(ExternalOutput, "media.ExternalOutput");

// We'll allow our audioQueue to be significantly larger than our video queue
// This is safe because A) audio is a lot smaller, so it isn't a big deal to hold on to a lot of it and
// B) our audio sample rate is typically 20 msec packets; video is anywhere from 33 to 100 msec (30 to 10 fps)
// Allowing the audio queue to hold more will help prevent loss of data when the video framerate is low.
ExternalOutput::ExternalOutput(const std::string& outputUrl) : audioQueue_(600, 60), videoQueue_(120, 60), inited_(false), video_stream_(NULL), audio_stream_(NULL),
    firstVideoTimestamp_(-1), firstAudioTimestamp_(-1), firstDataReceived_(-1), videoOffsetMsec_(-1), audioOffsetMsec_(-1)
{
    ELOG_DEBUG("Creating output to %s", outputUrl.c_str());

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

    unpackagedBufferpart_ = unpackagedBuffer_;
    lastFullIntraFrameRequest_ = 0;
    sinkfbSource_ = this;
    fbSink_ = NULL;
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
    ELOG_DEBUG("Destructing");

    // Stop our thread so we can safely nuke libav stuff and close our
    // our file.
    recording_ = false;
    cond_.notify_one();
    thread_.join();

    delete inputProcessor_;
    inputProcessor_ = NULL;

    if (audio_stream_ != NULL && video_stream_ != NULL && context_ != NULL){
        av_write_trailer(context_);
    }

    if (video_stream_ && video_stream_->codec != NULL){
        avcodec_close(video_stream_->codec);
    }

    if (audio_stream_ && audio_stream_->codec != NULL){
        avcodec_close(audio_stream_->codec);
    }

    if (context_ != NULL){
        avio_close(context_->pb);
        avformat_free_context(context_);
        context_ = NULL;
    }

    ELOG_DEBUG("Closed Successfully");
}

void ExternalOutput::receiveRawData(RawDataPacket& /*packet*/){
    return;
}


void ExternalOutput::writeAudioData(char* buf, int len){
    RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);

    if (firstAudioTimestamp_ == -1) {
        firstAudioTimestamp_ = head->getTimestamp();
    }

    timeval time;
    gettimeofday(&time, NULL);

    // Figure out our audio codec.
    if(context_->oformat->audio_codec == AV_CODEC_ID_NONE) {
        //We dont need any other payload at this time
        if(head->getPayloadType() == PCMU_8000_PT){
            context_->oformat->audio_codec = AV_CODEC_ID_PCM_MULAW;
        } else if (head->getPayloadType() == OPUS_48000_PT) {
            context_->oformat->audio_codec = AV_CODEC_ID_OPUS;
        }
    }

    initContext();

    if (audio_stream_ == NULL) {
        // not yet.
        return;
    }

    int ret = inputProcessor_->unpackageAudio(reinterpret_cast<unsigned char*>(buf), len, unpackagedAudioBuffer_);
    if (ret <= 0)
        return;

    long long currentTimestamp = head->getTimestamp();
    if (currentTimestamp - firstAudioTimestamp_ < 0) {
        // we wrapped.  add 2^32 to correct this.  We only handle a single wrap around since that's 13 hours of recording, minimum.
        currentTimestamp += 0xFFFFFFFF;
    }

    long long timestampToWrite = (currentTimestamp - firstAudioTimestamp_) / (audio_stream_->codec->time_base.den / audio_stream_->time_base.den);
    // Adjust for our start time offset
    timestampToWrite += audioOffsetMsec_ / (1000 / audio_stream_->time_base.den);   // in practice, our timebase den is 1000, so this operation is a no-op.

    /* ELOG_DEBUG("Writing audio frame %d with timestamp %u, normalized timestamp %u, audio offset msec %u, length %d, input timebase: %d/%d, target timebase: %d/%d", */
    /*            head->getSeqNumber(), head->getTimestamp(), timestampToWrite, audioOffsetMsec_, ret, */
    /*            audio_stream_->codec->time_base.num, audio_stream_->codec->time_base.den,    // timebase we requested */
    /*            audio_stream_->time_base.num, audio_stream_->time_base.den);                 // actual timebase */

    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = unpackagedAudioBuffer_;
    avpkt.size = ret;
    avpkt.pts = timestampToWrite;
    avpkt.stream_index = 1;
    av_write_frame(context_, &avpkt);
    av_free_packet(&avpkt);
}

void ExternalOutput::writeVideoData(char* buf, int len){
    RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);
    if (head->getPayloadType() == RED_90000_PT) {
        int totalLength = head->getHeaderLength();
        int rtpHeaderLength = totalLength;
        RedHeader *redhead = reinterpret_cast<RedHeader*>(buf + totalLength);
        if (redhead->payloadtype == VP8_90000_PT) {
            while (redhead->follow) {
                totalLength += redhead->getLength() + 4; // RED header
                redhead = reinterpret_cast<RedHeader*>(buf + totalLength);
            }
            // Parse RED packet to VP8 packet.
            // Copy RTP header
            memcpy(deliverMediaBuffer_, buf, rtpHeaderLength);
            // Copy payload data
            memcpy(deliverMediaBuffer_ + totalLength, buf + totalLength + 1, len - totalLength - 1);
            // Copy payload type
            RtpHeader *mediahead = reinterpret_cast<RtpHeader*>(deliverMediaBuffer_);
            mediahead->setPayloadType(redhead->payloadtype);
            buf = reinterpret_cast<char*>(deliverMediaBuffer_);
            len = len - 1 - totalLength + rtpHeaderLength;
        }
    }

    if (firstVideoTimestamp_ == -1) {
        firstVideoTimestamp_ = head->getTimestamp();
    }

    int gotUnpackagedFrame = false;
    int ret = inputProcessor_->unpackageVideo(reinterpret_cast<unsigned char*>(buf), len, unpackagedBufferpart_, &gotUnpackagedFrame);
    if (ret < 0){
        ELOG_ERROR("Error Unpackaging Video");
        return;
    }

    initContext();

    if (video_stream_ == NULL) {
        // could not init our context yet.
        return;
    }

    unpackagedSize_ += ret;
    unpackagedBufferpart_ += ret;

    if (gotUnpackagedFrame) {
        unpackagedBufferpart_ -= unpackagedSize_;

        long long currentTimestamp = head->getTimestamp();
        if (currentTimestamp - firstVideoTimestamp_ < 0) {
            // we wrapped.  add 2^32 to correct this.  We only handle a single wrap around since that's ~13 hours of recording, minimum.
            currentTimestamp += 0xFFFFFFFF;
        }

        long long timestampToWrite = (currentTimestamp - firstVideoTimestamp_) / (90000 / video_stream_->time_base.den);  // All of our video offerings are using a 90khz clock.

        // Adjust for our start time offset
        timestampToWrite += videoOffsetMsec_ / (1000 / video_stream_->time_base.den);   // in practice, our timebase den is 1000, so this operation is a no-op.

        /* ELOG_DEBUG("Writing video frame %d with timestamp %u, normalized timestamp %u, video offset msec %u, length %d, input timebase: %d/%d, target timebase: %d/%d", */
        /*            head->getSeqNumber(), head->getTimestamp(), timestampToWrite, videoOffsetMsec_, unpackagedSize_, */
        /*            video_stream_->codec->time_base.num, video_stream_->codec->time_base.den,    // timebase we requested */
        /*            video_stream_->time_base.num, video_stream_->time_base.den);                 // actual timebase */

        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = unpackagedBufferpart_;
        avpkt.size = unpackagedSize_;
        avpkt.pts = timestampToWrite;
        avpkt.stream_index = 0;
        av_write_frame(context_, &avpkt);
        av_free_packet(&avpkt);
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
            video_stream_ == NULL &&
            audio_stream_ == NULL) {

      AVCodec* videoCodec = avcodec_find_encoder(context_->oformat->video_codec);
        if (videoCodec==NULL){
            ELOG_ERROR("Could not find video codec");
            return false;
        }
        video_stream_ = avformat_new_stream (context_, videoCodec);
        video_stream_->id = 0;
        video_stream_->codec->codec_id = context_->oformat->video_codec;
        video_stream_->codec->width = 640;
        video_stream_->codec->height = 480;
        video_stream_->codec->time_base = (AVRational){1,30};   // A decent guess here suffices; if processing the file with ffmpeg,
                                                                // use -vsync 0 to force it not to duplicate frames.
        video_stream_->codec->pix_fmt = PIX_FMT_YUV420P;
        if (context_->oformat->flags & AVFMT_GLOBALHEADER){
            video_stream_->codec->flags|=CODEC_FLAG_GLOBAL_HEADER;
        }
        context_->oformat->flags |= AVFMT_VARIABLE_FPS;

        AVCodec* audioCodec = avcodec_find_encoder(context_->oformat->audio_codec);
        if (audioCodec==NULL){
            ELOG_ERROR("Could not find audio codec");
            return false;
        }

        audio_stream_ = avformat_new_stream (context_, audioCodec);
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

        if (avformat_write_header(context_, NULL) < 0){
            ELOG_ERROR("Error writing header");
            return false;
        }
    }

    return true;
}

void ExternalOutput::queueData(char* buffer, int length, packetType type){
    if (!recording_) {
        return;
    }

    RtcpHeader *head = reinterpret_cast<RtcpHeader*>(buffer);
    if (head->isRtcp()){
        return;
    }

    if (firstDataReceived_ == -1) {
        timeval time;
        gettimeofday(&time, NULL);
        firstDataReceived_ = (time.tv_sec * 1000) + (time.tv_usec / 1000);
        if (this->getAudioSinkSSRC() == 0){
          ELOG_DEBUG("No audio detected");
          context_->oformat->audio_codec = AV_CODEC_ID_PCM_MULAW;
        }
    }
    
    timeval time; 
    gettimeofday(&time, NULL);
    unsigned long long millis = (time.tv_sec * 1000) + (time.tv_usec / 1000);
    if (millis -lastFullIntraFrameRequest_ >FIR_INTERVAL_MS){
      this->sendFirPacket();
      lastFullIntraFrameRequest_ = millis;
    }

    if (type == VIDEO_PACKET){
        if(this->videoOffsetMsec_ == -1) {
            videoOffsetMsec_ = ((time.tv_sec * 1000) + (time.tv_usec / 1000)) - firstDataReceived_;
            ELOG_DEBUG("File %s, video offset msec: %llu", context_->filename, videoOffsetMsec_);
        }
        videoQueue_.pushPacket(buffer, length);
    }else{
        if(this->audioOffsetMsec_ == -1) {
            audioOffsetMsec_ = ((time.tv_sec * 1000) + (time.tv_usec / 1000)) - firstDataReceived_;
            ELOG_DEBUG("File %s, audio offset msec: %llu", context_->filename, audioOffsetMsec_);
        }
        audioQueue_.pushPacket(buffer, length);
    }

    if( audioQueue_.hasData() || videoQueue_.hasData()) {
        // One or both of our queues has enough data to write stuff out.  Notify our writer.
        cond_.notify_one();
    }
}

int ExternalOutput::sendFirPacket() {
    if (fbSink_ != NULL) {
        // ELOG_DEBUG("sending Full Intra-frame Request");
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
    boost::unique_lock<boost::mutex> lock(mtx_);
    cond_.wait(lock);
    while (audioQueue_.hasData()) {
      boost::shared_ptr<dataPacket> audioP = audioQueue_.popPacket();
      this->writeAudioData(audioP->data, audioP->length);
    }
    while (videoQueue_.hasData()) {
      boost::shared_ptr<dataPacket> videoP = videoQueue_.popPacket();
      this->writeVideoData(videoP->data, videoP->length);
    }
    if (!inited_ && firstDataReceived_!=-1){
      inited_ = true;
    }
    lock.unlock();
  }

  // Since we're bailing, let's completely drain our queues of all data.
  while (audioQueue_.getSize() > 0) {
    boost::shared_ptr<dataPacket> audioP = audioQueue_.popPacket(true); // ignore our minimum depth check
    this->writeAudioData(audioP->data, audioP->length);
  }
  while (videoQueue_.getSize() > 0) {
    boost::shared_ptr<dataPacket> videoP = videoQueue_.popPacket(true); // ignore our minimum depth check
    this->writeVideoData(videoP->data, videoP->length);
  }
}
}

