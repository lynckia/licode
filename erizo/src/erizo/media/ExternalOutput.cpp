#include "media/ExternalOutput.h"

#include <sys/time.h>

#include <string>
#include <cstring>

#include "lib/ClockUtils.h"

#include "./WebRtcConnection.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Parser.h"

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(ExternalOutput, "media.ExternalOutput");
ExternalOutput::ExternalOutput(const std::string& outputUrl)
  : audioQueue_(5.0, 10.0), videoQueue_(5.0, 10.0), inited_(false),
    video_stream_(NULL), audio_stream_(NULL), first_video_timestamp_(-1), first_audio_timestamp_(-1),
    first_data_received_(), video_offset_ms_(-1), audio_offset_ms_(-1), vp8SearchState_(lookingForStart),
    needToSendFir_(true) {
  ELOG_DEBUG("Creating output to %s", outputUrl.c_str());

  // TODO(pedro): these should really only be called once per application run
  av_register_all();
  avcodec_register_all();

  fec_receiver_.reset(webrtc::UlpfecReceiver::Create(this));

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
  sink_fb_source_ = this;
  fb_sink_ = nullptr;
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
  close();
}

void ExternalOutput::close() {
  if (!recording_) {
    return;
  }
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
bool ExternalOutput::OnRecoveredPacket(const uint8_t* rtp_packet, size_t rtp_packet_length) {
  videoQueue_.pushPacket((const char*)rtp_packet, rtp_packet_length);
  return true;
}

int32_t ExternalOutput::OnReceivedPayloadData(const uint8_t* payload_data, size_t payload_size,
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
  if (first_audio_timestamp_ != -1 && currentAudioSequenceNumber != lastAudioSequenceNumber_ + 1) {
      // Something screwy.  We should always see sequence numbers incrementing monotonically.
      ELOG_DEBUG("Unexpected audio sequence number; current %d, previous %d",
                  currentAudioSequenceNumber, lastAudioSequenceNumber_);
  }

  lastAudioSequenceNumber_ = currentAudioSequenceNumber;
  if (first_audio_timestamp_ == -1) {
      first_audio_timestamp_ = head->getTimestamp();
  }

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
  if (currentTimestamp - first_audio_timestamp_ < 0) {
      // we wrapped.  add 2^32 to correct this. We only handle a single wrap around
      // since that's 13 hours of recording, minimum.
      currentTimestamp += 0xFFFFFFFF;
  }

  long long timestampToWrite = (currentTimestamp - first_audio_timestamp_) /  // NOLINT
                                    (audio_stream_->codec->sample_rate / audio_stream_->time_base.den);
  // generally 48000 / 1000 for the denominator portion, at least for opus
  // Adjust for our start time offset

  // in practice, our timebase den is 1000, so this operation is a no-op.
  timestampToWrite += audio_offset_ms_ / (1000 / audio_stream_->time_base.den);

  AVPacket avpkt;
  av_init_packet(&avpkt);
  avpkt.data = reinterpret_cast<uint8_t*>(buf) + head->getHeaderLength();
  avpkt.size = len - head->getHeaderLength();
  avpkt.pts = timestampToWrite;
  avpkt.stream_index = 1;
  av_interleaved_write_frame(context_, &avpkt);   // takes ownership of the packet
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

    if (first_video_timestamp_ == -1) {
        first_video_timestamp_ = head->getTimestamp();
    }

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

    this->initContext();
    if (video_stream_ == NULL) {
      // could not init our context yet.
      return;
    }

    if (deliver) {
      unpackagedBufferpart_ -= unpackagedSize_;

      long long currentTimestamp = head->getTimestamp();  // NOLINT
      if (currentTimestamp - first_video_timestamp_ < 0) {
        // we wrapped.  add 2^32 to correct this.
        // We only handle a single wrap around since that's ~13 hours of recording, minimum.
        currentTimestamp += 0xFFFFFFFF;
      }

      // All of our video offerings are using a 90khz clock.
      long long timestampToWrite = (currentTimestamp - first_video_timestamp_) /  // NOLINT
                                                (90000 / video_stream_->time_base.den);

      // Adjust for our start time offset

      // in practice, our timebase den is 1000, so this operation is a no-op.
      timestampToWrite += video_offset_ms_ / (1000 / video_stream_->time_base.den);

      AVPacket avpkt;
      av_init_packet(&avpkt);
      avpkt.data = unpackagedBufferpart_;
      avpkt.size = unpackagedSize_;
      avpkt.pts = timestampToWrite;
      avpkt.stream_index = 0;
      av_interleaved_write_frame(context_, &avpkt);   // takes ownership of the packet
      unpackagedSize_ = 0;
      unpackagedBufferpart_ = unpackagedBuffer_;
    }
}

int ExternalOutput::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*audio_packet);
  this->queueData(copied_packet->data, copied_packet->length, AUDIO_PACKET);
  return 0;
}

int ExternalOutput::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*video_packet);
  // TODO(javierc): We should support higher layers, but it requires having an entire pipeline at this point
  if (!video_packet->belongsToSpatialLayer(0)) {
    return 0;
  }
  if (videoSourceSsrc_ == 0) {
    RtpHeader* h = reinterpret_cast<RtpHeader*>(copied_packet->data);
    videoSourceSsrc_ = h->getSSRC();
  }
  this->queueData(copied_packet->data, copied_packet->length, VIDEO_PACKET);
  return 0;
}

int ExternalOutput::deliverEvent_(MediaEventPtr event) {
  return 0;
}

bool ExternalOutput::initContext() {
  if (context_->oformat->video_codec != AV_CODEC_ID_NONE &&
            context_->oformat->audio_codec != AV_CODEC_ID_NONE &&
            video_stream_ == NULL &&
            audio_stream_ == NULL) {
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
    return;
  }

  if (first_data_received_ == time_point()) {
    first_data_received_ = clock::now();
    if (this->getAudioSinkSSRC() == 0) {
      ELOG_DEBUG("No audio detected");
      context_->oformat->audio_codec = AV_CODEC_ID_PCM_MULAW;
    }
  }
  if (needToSendFir_ && videoSourceSsrc_) {
    this->sendFirPacket();
    needToSendFir_ = false;
  }

  if (type == VIDEO_PACKET) {
    if (this->video_offset_ms_ == -1) {
      video_offset_ms_ = ClockUtils::durationToMs(clock::now() - first_data_received_);
      ELOG_DEBUG("File %s, video offset msec: %llu", context_->filename, video_offset_ms_);
    }

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
      if (0 == fec_receiver_->AddReceivedRedPacket(hackyHeader, (const uint8_t*)buffer, length, ULP_90000_PT)) {
        fec_receiver_->ProcessReceivedFec();
      }
    } else {
      videoQueue_.pushPacket(buffer, length);
    }
  } else {
    if (this->audio_offset_ms_ == -1) {
      audio_offset_ms_ = ClockUtils::durationToMs(clock::now() - first_data_received_);
      ELOG_DEBUG("File %s, audio offset msec: %llu", context_->filename, audio_offset_ms_);

      // Let's also take a moment to set our audio queue timebase.
      RtpHeader* h = reinterpret_cast<RtpHeader*>(buffer);
      if (h->getPayloadType() == PCMU_8000_PT) {
        audioQueue_.setTimebase(8000);
      } else if (h->getPayloadType() == OPUS_48000_PT) {
        audioQueue_.setTimebase(48000);
      }
    }
    audioQueue_.pushPacket(buffer, length);
  }

  if (audioQueue_.hasData() || videoQueue_.hasData()) {
    // One or both of our queues has enough data to write stuff out.  Notify our writer.
    cond_.notify_one();
  }
}

int ExternalOutput::sendFirPacket() {
    if (fb_sink_ != nullptr) {
      RtcpHeader thePLI;
      thePLI.setPacketType(RTCP_PS_Feedback_PT);
      thePLI.setBlockCount(1);
      thePLI.setSSRC(55543);
      thePLI.setSourceSSRC(videoSourceSsrc_);
      thePLI.setLength(2);
      char *buf = reinterpret_cast<char*>(&thePLI);
      int len = (thePLI.getLength() + 1) * 4;
      std::shared_ptr<DataPacket> pli_packet = std::make_shared<DataPacket>(0, buf, len, VIDEO_PACKET);
      fb_sink_->deliverFeedback(pli_packet);
      return len;
    }
    return -1;
}

void ExternalOutput::sendLoop() {
  while (recording_) {
    boost::unique_lock<boost::mutex> lock(mtx_);
    cond_.wait(lock);
    while (audioQueue_.hasData()) {
      boost::shared_ptr<DataPacket> audioP = audioQueue_.popPacket();
      this->writeAudioData(audioP->data, audioP->length);
    }
    while (videoQueue_.hasData()) {
      boost::shared_ptr<DataPacket> videoP = videoQueue_.popPacket();
      this->writeVideoData(videoP->data, videoP->length);
    }
    if (!inited_ && first_data_received_ != time_point()) {
      inited_ = true;
    }
  }

  // Since we're bailing, let's completely drain our queues of all data.
  while (audioQueue_.getSize() > 0) {
    boost::shared_ptr<DataPacket> audioP = audioQueue_.popPacket(true);  // ignore our minimum depth check
    this->writeAudioData(audioP->data, audioP->length);
  }
  while (videoQueue_.getSize() > 0) {
    boost::shared_ptr<DataPacket> videoP = videoQueue_.popPacket(true);  // ignore our minimum depth check
    this->writeVideoData(videoP->data, videoP->length);
  }
}
}  // namespace erizo
