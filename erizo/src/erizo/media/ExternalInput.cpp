#include "media/ExternalInput.h"

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>
#include <libavutil/time.h>

#include <cstdio>
#include <cstring>

#include "./WebRtcConnection.h"

namespace erizo {
DEFINE_LOGGER(ExternalInput, "media.ExternalInput");
ExternalInput::ExternalInput(const std::string& inputUrl):url_(inputUrl) {
  context_ = NULL;
  running_ = false;
  needTranscoding_ = false;
  lastPts_ = 0;
  lastAudioPts_ = 0;
}

ExternalInput::~ExternalInput() {
  ELOG_DEBUG("Destructor ExternalInput %s" , url_.c_str());
  ELOG_DEBUG("Closing ExternalInput");
  running_ = false;
  thread_.join();
  if (needTranscoding_)
    encodeThread_.join();
  av_free_packet(&avpacket_);
  if (context_ != NULL)
    avformat_free_context(context_);
  ELOG_DEBUG("ExternalInput closed");
}

int ExternalInput::init() {
  context_ = avformat_alloc_context();
  av_register_all();
  avcodec_register_all();
  avformat_network_init();
  // open rtsp
  av_init_packet(&avpacket_);
  avpacket_.data = NULL;
  ELOG_DEBUG("Trying to open input from url %s", url_.c_str());
  int res = avformat_open_input(&context_, url_.c_str(), NULL, NULL);
  char errbuff[500];
  ELOG_DEBUG("Opening input result %d", res);
  if (res != 0) {
    av_strerror(res, reinterpret_cast<char*>(&errbuff), 500);
    ELOG_ERROR("Error opening input %s", errbuff);
    return res;
  }
  res = avformat_find_stream_info(context_, NULL);
  if (res < 0) {
    av_strerror(res, reinterpret_cast<char*>(&errbuff), 500);
    ELOG_ERROR("Error finding stream info %s", errbuff);
    return res;
  }

  // VideoCodecInfo info;
  MediaInfo om;
  AVStream *st = nullptr;
  AVStream *audio_st = nullptr;

  int streamNo = av_find_best_stream(context_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (streamNo < 0) {
    ELOG_WARN("No Video stream found");
    // return streamNo;
  } else {
    om.hasVideo = true;
    video_stream_index_ = streamNo;
    st = context_->streams[streamNo];
  }

  int audioStreamNo = av_find_best_stream(context_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if (audioStreamNo < 0) {
    ELOG_WARN("No Audio stream found");
    ELOG_DEBUG("Has video, audio stream number %d. time base = %d / %d ",
               video_stream_index_, st->time_base.num, st->time_base.den);
    // return streamNo;
  } else {
    om.hasAudio = true;
    audio_stream_index_ = audioStreamNo;
    audio_st = context_->streams[audio_stream_index_];
    ELOG_DEBUG("Has Audio, audio stream number %d. time base = %d / %d ",
               audio_stream_index_, audio_st->time_base.num, audio_st->time_base.den);
    audio_time_base_ = audio_st->time_base.den;
    ELOG_DEBUG("Audio Time base %d", audio_time_base_);
    if (audio_st->codec->codec_id == AV_CODEC_ID_PCM_MULAW) {
      ELOG_DEBUG("PCM U8");
      om.audioCodec.sampleRate = 8000;
      om.audioCodec.codec = AUDIO_CODEC_PCM_U8;
      om.rtpAudioInfo.PT = PCMU_8000_PT;
    } else if (audio_st->codec->codec_id == AV_CODEC_ID_OPUS) {
      ELOG_DEBUG("OPUS");
      om.audioCodec.sampleRate = 48000;
      om.audioCodec.codec = AUDIO_CODEC_OPUS;
      om.rtpAudioInfo.PT = OPUS_48000_PT;
    }
    if (!om.hasVideo)
      st = audio_st;
  }


  if (st->codec->codec_id == AV_CODEC_ID_VP8 || !om.hasVideo) {
    ELOG_DEBUG("No need for video transcoding, already VP8");
    video_time_base_ = st->time_base.den;
    needTranscoding_ = false;
    decodedBuffer_.reset((unsigned char*) malloc(100000));
    MediaInfo om;
    om.processorType = PACKAGE_ONLY;
    if (audio_st) {
      if (audio_st->codec->codec_id == AV_CODEC_ID_PCM_MULAW) {
        ELOG_DEBUG("PCM U8");
        om.audioCodec.sampleRate = 8000;
        om.audioCodec.codec = AUDIO_CODEC_PCM_U8;
        om.rtpAudioInfo.PT = PCMU_8000_PT;
      } else if (audio_st->codec->codec_id == AV_CODEC_ID_OPUS) {
        ELOG_DEBUG("OPUS");
        om.audioCodec.sampleRate = 48000;
        om.audioCodec.codec = AUDIO_CODEC_OPUS;
        om.rtpAudioInfo.PT = OPUS_48000_PT;
      }
    }
    op_.reset(new OutputProcessor());
    op_->init(om, this);
  } else {
    needTranscoding_ = true;
    inCodec_.initDecoder(st->codec);

    bufflen_ = st->codec->width * st->codec->height * 3 / 2;
    decodedBuffer_.reset((unsigned char*) malloc(bufflen_));


    om.processorType = RTP_ONLY;
    om.videoCodec.codec = VIDEO_CODEC_VP8;
    om.videoCodec.bitRate = 1000000;
    om.videoCodec.width = st->codec->width;
    om.videoCodec.height = st->codec->height;
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

  thread_ = boost::thread(&ExternalInput::receiveLoop, this);
  running_ = true;
  if (needTranscoding_)
    encodeThread_ = boost::thread(&ExternalInput::encodeLoop, this);

  return true;
}

int ExternalInput::sendPLI() {
  if (op_) {
    op_->requestKeyframe();
  }
  return 0;
}

int ExternalInput::deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(fb_packet->data);
  if (chead->isFeedback()) {
    if (chead->getBlockCount() == 0 && (chead->getLength()+1) * 4  == fb_packet->length) {
      return 0;
    }
    char* moving_buf = fb_packet->data;
    int rtcp_length = 0;
    int total_length = 0;
    do {
      moving_buf += rtcp_length;
      chead = reinterpret_cast<RtcpHeader*>(moving_buf);
      rtcp_length = (ntohs(chead->length) + 1) * 4;
      total_length += rtcp_length;
      switch (chead->packettype) {
        case RTCP_RTP_Feedback_PT:
          // NACKs are already handled by MediaStream from the subscribers
          // sendPLI();
          break;
        case RTCP_PS_Feedback_PT:
          switch (chead->getBlockCount()) {
            case RTCP_PLI_FMT:
            case RTCP_FIR_FMT:
              sendPLI();
              break;
            case RTCP_AFB:
              char *unique_id = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
              if (!strncmp(unique_id, "REMB", 4)) {
                uint64_t bitrate = chead->getBrMantis() << chead->getBrExp();
                if (op_) {
                  op_->setTargetBitrate(bitrate);
                }
              }
              break;
          }
      }
    } while (total_length < fb_packet->length);
  }
  return 0;
}

void ExternalInput::receiveRtpData(unsigned char* rtpdata, int len) {
  if (auto video_sink = video_sink_.lock()) {
    RtcpHeader* head = reinterpret_cast<RtcpHeader*>(rtpdata);
    if (!head->isRtcp()) {
      if (getVideoSourceSSRC() == 0) {
        setVideoSourceSSRC(55543);
      }
    }
    std::shared_ptr<DataPacket> packet = std::make_shared<DataPacket>(0, reinterpret_cast<char*>(rtpdata),
        len, VIDEO_PACKET);
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    RTPPayloadVP8* payload = vp8_parser_.parseVP8(
      start_buffer, packet->length - rtp_header->getHeaderLength());
    if (!payload->frameType) {
      packet->is_keyframe = true;
    } else {
      packet->is_keyframe = false;
    }
    video_sink->deliverVideoData(packet);
  }
}

void ExternalInput::receiveLoop() {
  av_read_play(context_);  // play RTSP
  int gotDecodedFrame = 0;
  int length;
  startTime_ = av_gettime();

  ELOG_DEBUG("Start playing external input %s", url_.c_str() );
  while (av_read_frame(context_, &avpacket_) >= 0&& running_ == true) {
    AVPacket orig_pkt = avpacket_;
    if (needTranscoding_) {
      if (avpacket_.stream_index == video_stream_index_) {  // packet is video
        inCodec_.decodeVideo(avpacket_.data, avpacket_.size, decodedBuffer_.get(), bufflen_, &gotDecodedFrame);
        RawDataPacket packetR;
        if (gotDecodedFrame) {
          packetR.data = decodedBuffer_.get();
          packetR.length = bufflen_;
          packetR.type = VIDEO;
          queueMutex_.lock();
          packetQueue_.push(packetR);
          queueMutex_.unlock();
          gotDecodedFrame = 0;
        }
      }
    } else {
      if (avpacket_.stream_index == video_stream_index_) {  // packet is video
        // av_rescale(input, new_scale, old_scale)
        int64_t pts = av_rescale(lastPts_, 1000000, (long int) video_time_base_);  // NOLINT
        int64_t now = av_gettime() - startTime_;
        if (pts > now) {
          av_usleep(pts - now);
        }
        lastPts_ = avpacket_.pts;
        op_->packageVideo(avpacket_.data, avpacket_.size, decodedBuffer_.get(), avpacket_.pts);
      } else if (avpacket_.stream_index == audio_stream_index_) {  // packet is audio
        int64_t pts = av_rescale(lastAudioPts_, 1000000, (long int)audio_time_base_);  // NOLINT
        int64_t now = av_gettime() - startTime_;
        if (pts > now) {
          av_usleep(pts - now);
        }
        lastAudioPts_ = avpacket_.pts;
        length = op_->packageAudio(avpacket_.data, avpacket_.size, decodedBuffer_.get(), avpacket_.pts);
        if (length > 0) {
          std::shared_ptr<DataPacket> packet = std::make_shared<DataPacket>(0,
              reinterpret_cast<char*>(decodedBuffer_.get()), length, AUDIO_PACKET);
          if (auto audio_sink = audio_sink_.lock()) {
            audio_sink->deliverAudioData(packet);
          }
        }
      }
    }
    av_free_packet(&orig_pkt);
  }
  ELOG_DEBUG("Ended stream to play %s", url_.c_str());
  running_ = false;
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
}  // namespace erizo
