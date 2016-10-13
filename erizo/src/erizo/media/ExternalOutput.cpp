#include "media/ExternalOutput.h"

#include <sys/time.h>

#include <string>
#include <cstring>

#include "./WebRtcConnection.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Parser.h"

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(ExternalOutput, "media.ExternalOutput");
ExternalOutput::ExternalOutput(const std::string& outputUrl) : fec_receiver_(this), audioQueue_(5.0, 10.0),
videoQueue_(5.0, 10.0), inited_(false), video_stream_(NULL), audio_stream_(NULL), firstVideoTimestamp_(-1),
firstAudioTimestamp_(-1), firstDataReceived_(-1), vp8SearchState_(lookingForStart), needToSendFir_(true),
hasAudio(false), audioTimebaseSet(false), video_measurements(), audio_measurements(), SU(), lastAudioTS(0),
lastVideoTS(0), video_first_rtcp_ntp_timeMSW(0), audio_first_rtcp_ntp_timeMSW(0), video_first_rtcp_ntp_timeLSW(0),
audio_first_rtcp_ntp_timeLSW(0), video_first_rtcp_time(0), audio_first_rtcp_time(0) {
  ELOG_DEBUG("Creating output to %s", outputUrl.c_str());

  // TODO(pedro): these should really only be called once per application run
  av_register_all();
  avcodec_register_all();

  // our video timebase is easy: always 90 khz.  We'll set audio once we receive a packet and can inspect its header.
  videoQueue_.setTimebase(90000);

  context_ = avformat_alloc_context();
  if (context_ == NULL) {
    ELOG_ERROR("Error allocating memory for IO context");
  } else {
    outputUrl.copy(context_->filename, sizeof(context_->filename), 0);
    context_->oformat = av_guess_format(NULL,  context_->filename, NULL);
    if (!context_->oformat) {
      ELOG_ERROR("Error guessing format %s", context_->filename);
    } else {
      context_->oformat->video_codec = AV_CODEC_ID_VP8;
      context_->oformat->audio_codec = AV_CODEC_ID_NONE;
      // We'll figure this out once we start receiving data; it's either PCM or OPUS
    }
  }

  unpackagedBufferpart_ = unpackagedBuffer_;
  sinkfbSource_ = this;
  fbSink_ = NULL;
  unpackagedSize_ = 0;
  videoSourceSsrc_ = 0;
}

bool ExternalOutput::init() {
  MediaInfo m;
  m.hasVideo = false;
  m.hasAudio = false;
  thread_ = boost::thread(&ExternalOutput::sendLoop, this);
  recording_ = true;
  ELOG_DEBUG("Initialized successfully");
  return true;
}


ExternalOutput::~ExternalOutput() {
  ELOG_DEBUG("Destructing");

  // Stop our thread so we can safely nuke libav stuff and close our
  // our file.
  recording_ = false;
  cond_.notify_one();
  thread_.join();

  if (audio_stream_ != NULL && video_stream_ != NULL && context_ != NULL) {
    av_write_trailer(context_);
  }

  if (video_stream_ && video_stream_->codec != NULL) {
    avcodec_close(video_stream_->codec);
  }

  if (audio_stream_ && audio_stream_->codec != NULL) {
    avcodec_close(audio_stream_->codec);
  }

  if (context_ != NULL) {
    avio_close(context_->pb);
    avformat_free_context(context_);
    context_ = NULL;
  }

  ELOG_DEBUG("Closed Successfully");
}

void ExternalOutput::receiveRawData(const RawDataPacket& /*packet*/) {
  return;
}
// This is called by our fec_ object once it recovers a packet.
bool ExternalOutput::OnRecoveredPacket(const uint8_t* rtp_packet, int rtp_packet_length) {
  videoQueue_.pushPacket((const char*)rtp_packet, rtp_packet_length);
  return true;
}

int32_t ExternalOutput::OnReceivedPayloadData(const uint8_t* payload_data, const uint16_t payload_size,
                                              const webrtc::WebRtcRTPHeader* rtp_header) {
  // Unused by WebRTC's FEC implementation; just something we have to implement.
  return 0;
}

bool ExternalOutput::bufferCheck(RTPPayloadVP8* payload) {
  if (payload->dataLength + unpackagedSize_ >= UNPACKAGE_BUFFER_SIZE) {
    ELOG_ERROR("Not enough buffer. Dropping frame. Please adjust your UNPACKAGE_BUFFER_SIZE in ExternalOutput.h");
    unpackagedSize_ = 0;
    unpackagedBufferpart_ = unpackagedBuffer_;
    vp8SearchState_ = lookingForStart;
    return false;
  }
  return true;
}

void ExternalOutput::writeAudioData(char* buf, int len) {
  RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);
  uint16_t currentAudioSequenceNumber = head->getSeqNumber();
  if (firstAudioTimestamp_ != -1 && currentAudioSequenceNumber != lastAudioSequenceNumber_ + 1) {
      // Something screwy.  We should always see sequence numbers incrementing monotonically.
      ELOG_DEBUG("Unexpected audio sequence number; current %d, previous %d",
                  currentAudioSequenceNumber, lastAudioSequenceNumber_);
  }

  lastAudioSequenceNumber_ = currentAudioSequenceNumber;

  // Figure out our audio codec.
  if (context_->oformat->audio_codec == AV_CODEC_ID_NONE) {
    // We dont need any other payload at this time
    if (head->getPayloadType() == PCMU_8000_PT) {
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

  long long currentTimestamp = head->getTimestamp();  // NOLINT
  if (currentTimestamp - firstAudioTimestamp_ < 0) {
    // we wrapped.  add 2^32 to correct this.
    // We only handle a single wrap around since that's 13 hours of recording, minimum.
    ELOG_DEBUG("Audio Wrapped: current: %lld, first: %lld", currentTimestamp, firstAudioTimestamp_);
    currentTimestamp += 0xFFFFFFFF;
  }

  if (!SU.Slide(currentTimestamp, audio_measurements.rtcp_Packets, false)) {
    ELOG_WARN("Slide Audio Failed, TS: %lld", currentTimestamp);
    return;
  }

  // Distance from rtp timestamp and the last rtcp timestamp
  int delta_timestamp = currentTimestamp - audio_measurements.rtcp_Packets[0].getRtpTimestamp();
  // Distance of the last rtcp packet and the first one
  long long  addend = av_rescale_q(SU.NtpToMs(audio_measurements.rtcp_Packets[0].getNtpTimestampMSW(), //NOLINT
                   audio_measurements.rtcp_Packets[0].getNtpTimestampLSW()) - SU.NtpToMs(audio_first_rtcp_ntp_timeMSW,
                   audio_first_rtcp_ntp_timeLSW), (AVRational){1, 1000},
                   (AVRational){1, audio_stream_->codec->sample_rate});
  // Calculate the start offset of the A/V of the first rtcp packets using NTP timestamps.
  long long offset = SU.NtpToMs(audio_first_rtcp_ntp_timeMSW, audio_first_rtcp_ntp_timeLSW) - //NOLINT
                   SU.NtpToMs(video_first_rtcp_ntp_timeMSW, video_first_rtcp_ntp_timeLSW);
  // Rebase the offset into video TB
  offset = av_rescale_q(offset, (AVRational){1, 1000}, (AVRational){1, audio_stream_->codec->sample_rate});
  if (offset < 0) {
    offset = 0;
  }
  // Compute timestamp for packet
  currentTimestamp = offset + audio_first_rtcp_time + addend + delta_timestamp;

  if (firstAudioTimestamp_ == -1) {
    firstAudioTimestamp_ = currentTimestamp - offset;
  }

  long long timestampToWrite = (currentTimestamp - firstAudioTimestamp_) /  // NOLINT
                               (audio_stream_->codec->sample_rate / audio_stream_->time_base.den);
  // generally 48000 / 1000 for the denominator portion, at least for opus
  if (timestampToWrite <= lastAudioTS) {
          ELOG_WARN("AUDIO - Wanted to write a non increasing TS: discard this: %lld, last: %lld",
          timestampToWrite, lastAudioTS);
          return;
  }
  AVPacket avpkt;
  av_init_packet(&avpkt);
  avpkt.data = reinterpret_cast<uint8_t*>(buf) + head->getHeaderLength();
  avpkt.size = len - head->getHeaderLength();
  avpkt.pts = timestampToWrite;
  avpkt.stream_index = 1;
  int errCode = av_interleaved_write_frame(context_, &avpkt);
  if (errCode < 0) {
    ELOG_ERROR("Write audio frame failed ERRNO: %d", errCode);
  } else {
    lastAudioTS = timestampToWrite;
  }
}

void ExternalOutput::writeVideoData(char* buf, int len) {
    RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);

    uint16_t currentVideoSeqNumber = head->getSeqNumber();
    if (currentVideoSeqNumber != lastVideoSequenceNumber_ + 1) {
        // Something screwy.  We should always see sequence numbers incrementing monotonically.
        ELOG_DEBUG("Unexpected video sequence number; current %d, previous %d",
                  currentVideoSeqNumber, lastVideoSequenceNumber_);
        // Set our search state to look for the start of a frame, and discard what we currently have (if anything).
        // it's now worthless.
        vp8SearchState_ = lookingForStart;
        unpackagedSize_ = 0;
        unpackagedBufferpart_ = unpackagedBuffer_;
    }

    lastVideoSequenceNumber_ = currentVideoSeqNumber;


    // TODO(pedro) we should be tearing off RTP padding here, if it exists.  But WebRTC currently does not use padding.

    RtpVP8Parser parser;
    erizo::RTPPayloadVP8* payload = parser.parseVP8(reinterpret_cast<unsigned char*>(buf + head->getHeaderLength()),
                                                    len - head->getHeaderLength());

    bool endOfFrame = (head->getMarker() > 0);
    bool startOfFrame = payload->beginningOfPartition;

    bool deliver = false;
    switch (vp8SearchState_) {
    case lookingForStart:
      if (startOfFrame && endOfFrame) {
        // This packet is a standalone frame.  Send it on.  Look for start.
        unpackagedSize_ = 0;
        unpackagedBufferpart_ = unpackagedBuffer_;
        if (bufferCheck(payload)) {
          memcpy(unpackagedBufferpart_, payload->data, payload->dataLength);
          unpackagedSize_ += payload->dataLength;
          unpackagedBufferpart_ += payload->dataLength;
          deliver = true;
        }
      } else if (!startOfFrame && !endOfFrame) {
        // This is neither the start nor the end of a frame.  Reset our buffers.  Look for start.
        unpackagedSize_ = 0;
        unpackagedBufferpart_ = unpackagedBuffer_;
      } else if (startOfFrame && !endOfFrame) {
        // Found start frame.  Copy to buffers.  Look for our end.
        if (bufferCheck(payload)) {
          memcpy(unpackagedBufferpart_, payload->data, payload->dataLength);
          unpackagedSize_ += payload->dataLength;
          unpackagedBufferpart_ += payload->dataLength;
          vp8SearchState_ = lookingForEnd;
        }
      } else {  // (!startOfFrame && endOfFrame)
        // We got the end of a frame.  Reset our buffers.
        unpackagedSize_ = 0;
        unpackagedBufferpart_ = unpackagedBuffer_;
      }
      break;
    case lookingForEnd:
      if (startOfFrame && endOfFrame) {
        // Unexpected.  We were looking for the end of a frame, and got a whole new frame.
        // Reset our buffers, send this frame on, and go to the looking for start state.
        vp8SearchState_ = lookingForStart;
        unpackagedSize_ = 0;
        unpackagedBufferpart_ = unpackagedBuffer_;
        if (bufferCheck(payload)) {
          memcpy(unpackagedBufferpart_, payload->data, payload->dataLength);
          unpackagedSize_ += payload->dataLength;
          unpackagedBufferpart_ += payload->dataLength;
          deliver = true;
        }
      } else if (!startOfFrame && !endOfFrame) {
        // This is neither the start nor the end.  Add it to our unpackage buffer.
        if (bufferCheck(payload)) {
          memcpy(unpackagedBufferpart_, payload->data, payload->dataLength);
          unpackagedSize_ += payload->dataLength;
          unpackagedBufferpart_ += payload->dataLength;
        }
      } else if (startOfFrame && !endOfFrame) {
        // Unexpected.  We got the start of a frame.  Clear out our buffer, toss this payload in,
        // and continue looking for the end.
        unpackagedSize_ = 0;
        unpackagedBufferpart_ = unpackagedBuffer_;
        if (bufferCheck(payload)) {
          memcpy(unpackagedBufferpart_, payload->data, payload->dataLength);
          unpackagedSize_ += payload->dataLength;
          unpackagedBufferpart_ += payload->dataLength;
        }
      } else {  // (!startOfFrame && endOfFrame)
        // Got the end of a frame.  Let's deliver and start looking for the start of a frame.
        vp8SearchState_ = lookingForStart;
        if (bufferCheck(payload)) {
          memcpy(unpackagedBufferpart_, payload->data, payload->dataLength);
          unpackagedSize_ += payload->dataLength;
          unpackagedBufferpart_ += payload->dataLength;
          deliver = true;
        }
      }
      break;
    }

    delete payload;

    if (deliver) {
      unpackagedBufferpart_ -= unpackagedSize_;

      this->initContext();
      if (video_stream_ == NULL) {
        // could not init our context yet.
        return;
      }

      long long currentTimestamp = head->getTimestamp();  // NOLINT
      if (currentTimestamp - firstVideoTimestamp_ < 0) {
        // we wrapped.  add 2^32 to correct this.
        // We only handle a single wrap around since that's ~13 hours of recording, minimum.
        currentTimestamp += 0xFFFFFFFF;
      }

      if (!SU.Slide(currentTimestamp, video_measurements.rtcp_Packets, true)) {
        ELOG_WARN("Slide Video Failed, TS: %lld", currentTimestamp);
        unpackagedSize_ = 0;
        unpackagedBufferpart_ = unpackagedBuffer_;
        return;
      }
      // Distance from rtp timestamp and the last rtcp timestamp
      int delta_timestamp = currentTimestamp - video_measurements.rtcp_Packets[0].getRtpTimestamp();  // 1/90000
      // Distance of the last rtcp packet and the first one
      long long addend = av_rescale_q(SU.NtpToMs(video_measurements.rtcp_Packets[0].getNtpTimestampMSW(), //NOLINT
                       video_measurements.rtcp_Packets[0].getNtpTimestampLSW()) -
                       SU.NtpToMs(video_first_rtcp_ntp_timeMSW, video_first_rtcp_ntp_timeLSW), (AVRational){1, 1000},
                       (AVRational){1, 90000});
      long long offset = 0; //NOLINT
      if (hasAudio) {
        // Calculate the start offset of the A/V of the first rtcp packets using NTP timestamps.
        offset = SU.NtpToMs(video_first_rtcp_ntp_timeMSW, video_first_rtcp_ntp_timeLSW) -
                 SU.NtpToMs(audio_first_rtcp_ntp_timeMSW, audio_first_rtcp_ntp_timeLSW);
        // Rebase the offset into video TB
        offset = av_rescale_q(offset, (AVRational){1, 1000}, (AVRational){1, 90000});
        if (offset <0) {
          offset = 0;
        }
      }

      if (firstVideoTimestamp_ == -1) {
        firstVideoTimestamp_ = currentTimestamp - offset;
        needToSendFir_ = true;
      }

      // Final timestamp of the video frame
      currentTimestamp = offset + video_first_rtcp_time + addend + delta_timestamp;

      // All of our video offerings are using a 90khz clock.
      long long timestampToWrite = (currentTimestamp - firstVideoTimestamp_) /  // NOLINT
                                                (90000 / video_stream_->time_base.den);

      if (timestampToWrite <= lastVideoTS) {
        ELOG_WARN("VIDEO - Wanted to write a non increasing TS - this: %lld, last: %lld", timestampToWrite, lastVideoTS);
        timestampToWrite = lastVideoTS + 10;
      }

      AVPacket avpkt;
      av_init_packet(&avpkt);
      avpkt.data = unpackagedBufferpart_;
      avpkt.size = unpackagedSize_;
      avpkt.pts = timestampToWrite;
      avpkt.stream_index = 0;
      int errCode = av_interleaved_write_frame(context_, &avpkt);
      if (errCode < 0) {
        ELOG_ERROR("Write video frame failed ERRNO: %d", errCode);
      } else {
        lastVideoTS = timestampToWrite;
      }
      unpackagedSize_ = 0;
      unpackagedBufferpart_ = unpackagedBuffer_;
    }
}

int ExternalOutput::deliverAudioData_(char* buf, int len) {
  this->queueData(buf, len, AUDIO_PACKET);
  return 0;
}

int ExternalOutput::deliverVideoData_(char* buf, int len) {
  if (videoSourceSsrc_ == 0) {
    RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
    if (h->getPayloadType() == RED_90000_PT || h->getPayloadType() == VP8_90000_PT) {
      videoSourceSsrc_ = h->getSSRC();
    } else {
      ELOG_DEBUG("Wrong payload to set videoSSRC. %u != 116 (RED) || 100 (VP8). Probably an RTCP packet.",
      h->getPayloadType());
    }
  }
  this->queueData(buf, len, VIDEO_PACKET);
  return 0;
}


bool ExternalOutput::initContext() {
  if (context_->oformat->video_codec != AV_CODEC_ID_NONE && context_->oformat->audio_codec != AV_CODEC_ID_NONE &&
  video_stream_ == NULL && audio_stream_ == NULL) {
    AVCodec* videoCodec = avcodec_find_encoder(context_->oformat->video_codec);
    if (videoCodec == NULL) {
      ELOG_ERROR("Could not find video codec");
      return false;
    }
    video_stream_ = avformat_new_stream(context_, videoCodec);
    video_stream_->id = 0;
    video_stream_->codec->codec_id = context_->oformat->video_codec;
    video_stream_->codec->width = 640;
    video_stream_->codec->height = 480;
    video_stream_->time_base = (AVRational) { 1, 30 };
    // A decent guess here suffices; if processing the file with ffmpeg,
      // use -vsync 0 to force it not to duplicate frames.
    video_stream_->codec->pix_fmt = PIX_FMT_YUV420P;
    if (context_->oformat->flags & AVFMT_GLOBALHEADER) {
      video_stream_->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    context_->oformat->flags |= AVFMT_VARIABLE_FPS;

    AVCodec* audioCodec = avcodec_find_encoder(context_->oformat->audio_codec);
    if (audioCodec == NULL) {
      ELOG_ERROR("Could not find audio codec");
      return false;
    }

    audio_stream_ = avformat_new_stream(context_, audioCodec);
    audio_stream_->id = 1;
    audio_stream_->codec->codec_id = context_->oformat->audio_codec;
    audio_stream_->codec->sample_rate = context_->oformat->audio_codec == AV_CODEC_ID_PCM_MULAW ? 8000 : 48000;
    // TODO(pedro) is it always 48 khz for opus?
    audio_stream_->time_base = (AVRational) { 1, audio_stream_->codec->sample_rate };
    audio_stream_->codec->channels = context_->oformat->audio_codec == AV_CODEC_ID_PCM_MULAW ? 1 : 2;
    // TODO(pedro) is it always two channels for opus?
    if (context_->oformat->flags & AVFMT_GLOBALHEADER) {
      audio_stream_->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    context_->streams[0] = video_stream_;
    context_->streams[1] = audio_stream_;
    if (avio_open(&context_->pb, context_->filename, AVIO_FLAG_WRITE) < 0) {
      ELOG_ERROR("Error opening output file");
      return false;
    }

    if (avformat_write_header(context_, NULL) < 0) {
      ELOG_ERROR("Error writing header");
      return false;
    }
  }
  return true;
}

void ExternalOutput::queueData(char* buffer, int length, packetType type) {
  if (!recording_) {
    return;
  }

  RtcpHeader *head = reinterpret_cast<RtcpHeader*>(buffer);
  if (head->isRtcp()) {
    if (videoSourceSsrc_ != 0) {
      if (head->getSSRC() == videoSourceSsrc_) {
        // VIDEO
        ELOG_DEBUG("Saved SR Video");
        video_measurements.rtcp_Packets.push_back(*head);
        if (video_first_rtcp_time == 0) {
          video_first_rtcp_time = head->getRtpTimestamp();
          video_first_rtcp_ntp_timeMSW = head->getNtpTimestampMSW();
          video_first_rtcp_ntp_timeLSW = head->getNtpTimestampLSW();
        }
    } else {
        // AUDIO
        ELOG_DEBUG("Saved SR Audio");
        audio_measurements.rtcp_Packets.push_back(*head);
        if (audio_first_rtcp_time == 0) {
          audio_first_rtcp_time = head->getRtpTimestamp();
          audio_first_rtcp_ntp_timeMSW = head->getNtpTimestampMSW();
          audio_first_rtcp_ntp_timeLSW = head->getNtpTimestampLSW();
        }
      }
    }
    return;
  }

  if (firstDataReceived_ == -1) {
    timeval time;
    gettimeofday(&time, NULL);
    firstDataReceived_ = (time.tv_sec * 1000) + (time.tv_usec / 1000);
    if (this->getAudioSinkSSRC() == 0) {
      ELOG_DEBUG("No audio detected");
      context_->oformat->audio_codec = AV_CODEC_ID_PCM_MULAW;
      hasAudio = false;
    } else {
      hasAudio = true;
    }
  }
  if (needToSendFir_ && videoSourceSsrc_) {
    this->sendFirPacket();
    needToSendFir_ = false;
  }

  if (type == VIDEO_PACKET) {
    // If this is a red header, let's push it to our fec_receiver_, which will spit out frames in one
    // of our other callbacks.
    // Otherwise, just stick it straight into the video queue.
    RtpHeader* h = reinterpret_cast<RtpHeader*>(buffer);
    if (h->getPayloadType() == RED_90000_PT) {
      // The only things AddReceivedRedPacket uses are headerLength and sequenceNumber.
      // Unfortunately the amount of crap
      // we would have to pull in from the WebRtc project to fully construct
      // a webrtc::RTPHeader object is obscene.  So
      // let's just do this hacky fix.
      webrtc::RTPHeader hackyHeader;
      hackyHeader.headerLength = h->getHeaderLength();
      hackyHeader.sequenceNumber = h->getSeqNumber();

      // AddReceivedRedPacket returns 0 if there's data to process
      if (0 == fec_receiver_.AddReceivedRedPacket(hackyHeader, (const uint8_t*)buffer, length, ULP_90000_PT)) {
        fec_receiver_.ProcessReceivedFec();
      }
    } else {
      videoQueue_.pushPacket(buffer, length);
    }
  } else {
    if (audioTimebaseSet == false) {
      // Let's also take a moment to set our audio queue timebase.
      RtpHeader* h = reinterpret_cast<RtpHeader*>(buffer);
      if (h->getPayloadType() == PCMU_8000_PT) {
        audioQueue_.setTimebase(8000);
      } else if (h->getPayloadType() == OPUS_48000_PT) {
        audioQueue_.setTimebase(48000);
      }
      audioTimebaseSet = true;
    }
    audioQueue_.pushPacket(buffer, length);
  }

  if (audioQueue_.hasData() || videoQueue_.hasData()) {
    // One or both of our queues has enough data to write stuff out.  Notify our writer.
    cond_.notify_one();
  }
}

int ExternalOutput::sendFirPacket() {
  if (fbSink_ != NULL) {
    RtcpHeader thePLI;
    thePLI.setPacketType(RTCP_PS_Feedback_PT);
    thePLI.setBlockCount(1);
    thePLI.setSSRC(55543);
    thePLI.setSourceSSRC(videoSourceSsrc_);
    thePLI.setLength(2);
    char *buf = reinterpret_cast<char*>(&thePLI);
    int len = (thePLI.getLength() + 1) * 4;
    fbSink_->deliverFeedback(reinterpret_cast<char*>(buf), len);
    return len;
  }
  return -1;
}

void ExternalOutput::sendLoop() {
  while (recording_) {
    boost::unique_lock<boost::mutex> lock(mtx_);
    cond_.wait(lock);
    if (hasAudio) {
      while (audioQueue_.hasData() && audio_first_rtcp_time != 0 && video_first_rtcp_time != 0) {
        boost::shared_ptr<dataPacket> audioP = audioQueue_.popPacket();
        this->writeAudioData(audioP->data, audioP->length);
      }
      while (videoQueue_.hasData() && audio_first_rtcp_time != 0 && video_first_rtcp_time != 0) {
        boost::shared_ptr<dataPacket> videoP = videoQueue_.popPacket();
        this->writeVideoData(videoP->data, videoP->length);
      }
    } else {
      while (videoQueue_.hasData() && video_first_rtcp_time != 0) {
        boost::shared_ptr<dataPacket> videoP = videoQueue_.popPacket();
        this->writeVideoData(videoP->data, videoP->length);
      }
    }
    if (!inited_ && firstDataReceived_ !=-1) {
      inited_ = true;
    }
  }

  // Since we're bailing, let's completely drain our queues of all data.
  while (audioQueue_.getSize() > 0) {
    boost::shared_ptr<dataPacket> audioP = audioQueue_.popPacket(true);  // ignore our minimum depth check
    this->writeAudioData(audioP->data, audioP->length);
  }
  while (videoQueue_.getSize() > 0) {
    boost::shared_ptr<dataPacket> videoP = videoQueue_.popPacket(true);  // ignore our minimum depth check
    this->writeVideoData(videoP->data, videoP->length);
  }
}
}  // namespace erizo
