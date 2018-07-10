#include "media/ExternalOutput.h"

#include <sys/time.h>

#include <string>
#include <cstring>

#include "lib/ClockUtils.h"

#include "./WebRtcConnection.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Parser.h"

#include "rtp/QualityFilterHandler.h"
#include "rtp/LayerBitrateCalculationHandler.h"

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(ExternalOutput, "media.ExternalOutput");
ExternalOutput::ExternalOutput(std::shared_ptr<Worker> worker, const std::string& output_url,
                               const std::vector<RtpMap> rtp_mappings,
                               const std::vector<erizo::ExtMap> ext_mappings)
  : worker_{worker}, pipeline_{Pipeline::create()}, audio_queue_{5.0, 10.0}, video_queue_{5.0, 10.0},
    initialized_context_{false}, video_source_ssrc_{0},
    first_video_timestamp_{-1}, first_audio_timestamp_{-1},
    first_data_received_{}, video_offset_ms_{-1}, audio_offset_ms_{-1},
    need_to_send_fir_{true}, rtp_mappings_{rtp_mappings}, input_video_codec_id_{AV_CODEC_ID_NONE},
    input_audio_codec_id_{AV_CODEC_ID_NONE}, need_video_transcode_{false}, need_audio_transcode_{false},
    need_audio_resample_{false}, ext_processor_{ext_mappings}, pipeline_initialized_{false} {
  ELOG_DEBUG("Creating output to %s", output_url.c_str());

  fb_sink_ = nullptr;
  sink_fb_source_ = this;

  // TODO(pedro): these should really only be called once per application run
  av_register_all();
  avcodec_register_all();
  avformat_network_init();

  fec_receiver_.reset(webrtc::UlpfecReceiver::Create(this));
  stats_ = std::make_shared<Stats>();
  quality_manager_ = std::make_shared<QualityManager>();

  for (auto rtp_map : rtp_mappings_) {
    switch (rtp_map.media_type) {
      case AUDIO_TYPE:
        audio_maps_[rtp_map.payload_type] = rtp_map;
        break;
      case VIDEO_TYPE:
        video_maps_[rtp_map.payload_type] = rtp_map;
        break;
      case OTHER:
        break;
    }
  }

  context_ = avformat_alloc_context();
  if (context_ == nullptr) {
    ELOG_ERROR("Error allocating memory for IO context");
  } else {
    output_url.copy(context_->filename, sizeof(context_->filename), 0);

    context_->oformat = av_guess_format(nullptr, context_->filename, nullptr);
    if (!context_->oformat) {
      ELOG_ERROR("Error guessing format %s", context_->filename);
    }
  }

  // Set a fixed extension map to parse video orientation
  // TODO(yannistseng): Update extension maps dymaically from SDP info
  std::shared_ptr<SdpInfo> sdp = std::make_shared<SdpInfo>(rtp_mappings_);
  ExtMap anExt(4, "urn:3gpp:video-orientation");
  anExt.mediaType = VIDEO_TYPE;
  sdp->extMapVector.push_back(anExt);
  ext_processor_.setSdpInfo(sdp);
}

bool ExternalOutput::init() {
  MediaInfo m;
  m.hasVideo = false;
  m.hasAudio = false;
  recording_ = true;
  asyncTask([] (std::shared_ptr<ExternalOutput> output) {
    output->initializePipeline();
  });
  thread_ = boost::thread(&ExternalOutput::sendLoop, this);
  ELOG_DEBUG("Initialized successfully");
  return true;
}


ExternalOutput::~ExternalOutput() {
  ELOG_DEBUG("Destructing");
}

void ExternalOutput::close() {
  std::shared_ptr<ExternalOutput> shared_this = shared_from_this();
  asyncTask([shared_this] (std::shared_ptr<ExternalOutput> connection) {
    shared_this->syncClose();
  });
}

void ExternalOutput::syncClose() {
  if (!recording_) {
    return;
  }

  cond_.notify_one();
  thread_.join();

  if (context_ != nullptr) {
    av_write_trailer(context_);
  }
  if (video_decoder_.isInitialized()) {
    video_decoder_.closeCodec();
  }
  if (video_encoder_.isInitialized()) {
    video_encoder_.closeCodec();
  }
  if (audio_decoder_.isInitialized()) {
    audio_decoder_.closeCodec();
  }
  if (audio_encoder_.isInitialized()) {
    audio_encoder_.closeCodec();
  }
  if (context_ != nullptr) {
    avio_close(context_->pb);
    avformat_free_context(context_);
  }

  pipeline_initialized_ = false;
  recording_ = false;

  ELOG_INFO("Closed Successfully");
}

void ExternalOutput::asyncTask(std::function<void(std::shared_ptr<ExternalOutput>)> f) {
  std::weak_ptr<ExternalOutput> weak_this = shared_from_this();
  worker_->task([weak_this, f] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
  });
}

void ExternalOutput::receiveRawData(const RawDataPacket& /*packet*/) {
  return;
}
// This is called by our fec_ object once it recovers a packet.
bool ExternalOutput::OnRecoveredPacket(const uint8_t* rtp_packet, size_t rtp_packet_length) {
  video_queue_.pushPacket((const char*)rtp_packet, rtp_packet_length);
  return true;
}

int32_t ExternalOutput::OnReceivedPayloadData(const uint8_t* payload_data, size_t payload_size,
                                              const webrtc::WebRtcRTPHeader* rtp_header) {
  // Unused by WebRTC's FEC implementation; just something we have to implement.
  return 0;
}

void ExternalOutput::writeAudioData(char* buf, int len) {
  RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);
  uint16_t current_audio_sequence_number = head->getSeqNumber();
  if (first_audio_timestamp_ != -1 && current_audio_sequence_number != last_audio_sequence_number_ + 1) {
      // Something screwy.  We should always see sequence numbers incrementing monotonically.
      ELOG_DEBUG("Unexpected audio sequence number; current %d, previous %d",
                  current_audio_sequence_number, last_audio_sequence_number_);
  }

  last_audio_sequence_number_ = current_audio_sequence_number;
  if (first_audio_timestamp_ == -1) {
      first_audio_timestamp_ = head->getTimestamp();
  }

  auto map_iterator = audio_maps_.find(head->getPayloadType());
  if (map_iterator != audio_maps_.end()) {
    updateAudioCodec(map_iterator->second);
  }

  if (!initContext()) {
    return;
  }

  int64_t current_timestamp = head->getTimestamp();  // NOLINT
  int64_t timestamp_to_write = (current_timestamp - first_audio_timestamp_);
  timestamp_to_write += av_rescale(audio_offset_ms_, 1000, audio_map_.clock_rate);

  AVPacket av_packet;
  av_init_packet(&av_packet);
  av_packet.data = reinterpret_cast<uint8_t*>(buf) + head->getHeaderLength();
  av_packet.size = len - head->getHeaderLength();
  av_packet.pts = timestamp_to_write;
  av_packet.stream_index = 1;

  if (av_packet.size > 0 && need_audio_transcode_) {
    if (audio_decoder_.decode(audio_decoder_.frame_, &av_packet)) {
      EncodeCB encode_callback = [this](AVPacket *pkt, AVFrame *frame, bool should_write){
        av_frame_unref(frame);
        this->writePacket(pkt, audio_st_, should_write);
      };
      if (need_audio_resample_) {
        if (audio_encoder_.resampleAudioFrame(audio_decoder_.frame_, &audio_encoder_.resampled_frame_)) {
          static int64_t i = 0;
          audio_encoder_.resampled_frame_->pts = i;
          i += audio_encoder_.resampled_frame_->nb_samples;
          audio_encoder_.encode(audio_encoder_.resampled_frame_, &av_packet, encode_callback);
        }
        av_frame_unref(audio_decoder_.frame_);
      } else {
        audio_encoder_.encode(audio_decoder_.frame_, &av_packet, encode_callback);
      }
    }
  } else if (av_packet.size > 0) {
    this->writePacket(&av_packet, audio_st_, true);
  }
  av_packet_unref(&av_packet);
}

void ExternalOutput::writeVideoData(char* buf, int len) {
  RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);

  uint16_t current_video_sequence_number = head->getSeqNumber();
  if (current_video_sequence_number != last_video_sequence_number_ + 1) {
    // Something screwy.  We should always see sequence numbers incrementing monotonically.
    ELOG_DEBUG("Unexpected video sequence number; current %d, previous %d",
              current_video_sequence_number, last_video_sequence_number_);
    // Restart the depacketizer so it looks for the start of a frame
    if (depacketizer_!= nullptr) {
      depacketizer_->reset();
    }
  }

  last_video_sequence_number_ = current_video_sequence_number;

  if (first_video_timestamp_ == -1) {
    first_video_timestamp_ = head->getTimestamp();
  }
  auto map_iterator = video_maps_.find(head->getPayloadType());
  if (map_iterator != video_maps_.end()) {
    updateVideoCodec(map_iterator->second);
    if (map_iterator->second.encoding_name == "VP8" || map_iterator->second.encoding_name == "H264") {
      maybeWriteVideoPacket(buf, len);
    }
  }
}

void ExternalOutput::updateAudioCodec(RtpMap map) {
  if (input_audio_codec_id_ != AV_CODEC_ID_NONE) {
    return;
  }
  audio_map_ = map;
  if (map.encoding_name == "opus") {
    input_audio_codec_id_ = AV_CODEC_ID_OPUS;
  } else if (map.encoding_name == "PCMU") {
    input_audio_codec_id_ = AV_CODEC_ID_PCM_MULAW;
  }
}

void ExternalOutput::updateVideoCodec(RtpMap map) {
  if (input_video_codec_id_ != AV_CODEC_ID_NONE) {
    return;
  }
  video_map_ = map;
  if (map.encoding_name == "VP8") {
    depacketizer_.reset(new Vp8Depacketizer());
    input_video_codec_id_ = AV_CODEC_ID_VP8;
  } else if (map.encoding_name == "H264") {
    depacketizer_.reset(new H264Depacketizer());
    input_video_codec_id_ = AV_CODEC_ID_H264;
  }
}

void ExternalOutput::maybeWriteVideoPacket(char* buf, int len) {
  RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);
  depacketizer_->fetchPacket((unsigned char*)buf, len);
  bool deliver = depacketizer_->processPacket();

  if (!initContext()) {
    depacketizer_->reset();
    return;
  }

  if (deliver) {
    long long current_timestamp = head->getTimestamp();  // NOLINT
    if (current_timestamp - first_video_timestamp_ < 0) {
      // we wrapped.  add 2^32 to correct this.
      // We only handle a single wrap around since that's ~13 hours of recording, minimum.
      current_timestamp += 0xFFFFFFFF;
    }

    long long timestamp_to_write = (current_timestamp - first_video_timestamp_);// NOLINT
    timestamp_to_write += av_rescale(video_offset_ms_, 1000, video_map_.clock_rate);

    AVPacket av_packet;
    av_init_packet(&av_packet);
    av_packet.data = depacketizer_->frame();
    av_packet.size = depacketizer_->frameSize();
    av_packet.pts = timestamp_to_write;
    av_packet.stream_index = 0;
    if (depacketizer_->isKeyframe()) {
      av_packet.flags |= AV_PKT_FLAG_KEY;
    }

    if (av_packet.size > 0 && need_video_transcode_) {
      if (video_decoder_.decode(video_decoder_.frame_, &av_packet)) {
        auto done_callback = [this](AVPacket *pkt, AVFrame *frame, bool should_write){
          av_frame_unref(frame);
          this->writePacket(pkt, video_st_, should_write);
        };
        video_encoder_.encode(video_decoder_.frame_, &av_packet, done_callback);
      }
    } else {
      this->writePacket(&av_packet, video_st_, av_packet.size > 0);
    }
    av_packet_unref(&av_packet);
    depacketizer_->reset();
  }
}

void ExternalOutput::writePacket(AVPacket *pkt, AVStream *st, bool should_write) {
  if (should_write) {
    const char *media_type;

    if (st->id == 0) {
      media_type = "video";
      av_packet_rescale_ts(pkt, AVRational{1, static_cast<int>(video_map_.clock_rate)},
            video_st_->time_base);
    } else {
      media_type = "audio";
      av_packet_rescale_ts(pkt, audio_encoder_.codec_context_->time_base, audio_st_->time_base);
    }
    pkt->stream_index = st->id;

    int64_t pts = pkt->pts;
    int64_t dts = pkt->dts;
    int64_t dl  = pkt->dts;

    int ret = av_interleaved_write_frame(context_, pkt);
    if (ret != 0) {
      ELOG_ERROR("av_interleaved_write_frame pts: %d failed with: %d %s",
          pts, ret, av_err2str_cpp(ret));
    } else {
      ELOG_DEBUG("Writed %s packet with pts: %lld, dts: %lld, duration: %lld",
          media_type, pts, dts, dl);
    }
  }
  av_packet_unref(pkt);
}

void ExternalOutput::notifyUpdateToHandlers() {
  asyncTask([] (std::shared_ptr<ExternalOutput> output) {
    output->pipeline_->notifyUpdate();
  });
}

void ExternalOutput::initializePipeline() {
  stats_->getNode()["total"].insertStat("senderBitrateEstimation",
      CumulativeStat{static_cast<uint64_t>(kExternalOutputMaxBitrate)});

  handler_manager_ = std::make_shared<HandlerManager>(shared_from_this());
  pipeline_->addService(handler_manager_);
  pipeline_->addService(quality_manager_);
  pipeline_->addService(stats_);

  pipeline_->addFront(std::make_shared<LayerBitrateCalculationHandler>());
  pipeline_->addFront(std::make_shared<QualityFilterHandler>());

  pipeline_->addFront(std::make_shared<ExternalOuputWriter>(shared_from_this()));
  pipeline_->finalize();
  pipeline_initialized_ = true;
}

void ExternalOutput::write(std::shared_ptr<DataPacket> packet) {
  queueData(packet->data, packet->length, packet->type);
}

void ExternalOutput::queueDataAsync(std::shared_ptr<DataPacket> copied_packet) {
  asyncTask([copied_packet] (std::shared_ptr<ExternalOutput> this_ptr) {
    if (!this_ptr->pipeline_initialized_) {
      return;
    }
    this_ptr->pipeline_->write(std::move(copied_packet));
  });
}

int ExternalOutput::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*audio_packet);
  copied_packet->type = AUDIO_PACKET;
  queueDataAsync(copied_packet);
  return 0;
}

int ExternalOutput::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  if (video_source_ssrc_ == 0) {
    RtpHeader* h = reinterpret_cast<RtpHeader*>(video_packet->data);
    video_source_ssrc_ = h->getSSRC();
  }

  std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*video_packet);
  copied_packet->type = VIDEO_PACKET;
  ext_processor_.processRtpExtensions(copied_packet);
  queueDataAsync(copied_packet);
  return 0;
}

int ExternalOutput::deliverEvent_(MediaEventPtr event) {
  auto output_ptr = shared_from_this();
  worker_->task([output_ptr, event]{
    if (!output_ptr->pipeline_initialized_) {
      return;
    }
    output_ptr->pipeline_->notifyEvent(event);
  });
  return 1;
}

bool ExternalOutput::initContext() {
  if (input_video_codec_id_ != AV_CODEC_ID_NONE &&
            input_audio_codec_id_ != AV_CODEC_ID_NONE &&
            initialized_context_ == false) {
    need_to_send_fir_ = true;
    video_queue_.setTimebase(video_map_.clock_rate);
    if (avio_open(&context_->pb, context_->filename, AVIO_FLAG_WRITE) < 0) {
      ELOG_ERROR("Error opening output context.");
    } else {
      AVCodecID output_video_codec_id = this->bestMatchOutputCodecId(input_video_codec_id_);
      AVCodecID output_audio_codec_id = this->bestMatchOutputCodecId(input_audio_codec_id_);

      if (input_video_codec_id_ == output_video_codec_id) {
        ELOG_INFO("Video input codec matches output codec, don't need to transcode.");
      } else {
        need_video_transcode_ = true;
      }
      if (input_audio_codec_id_ == output_audio_codec_id) {
        ELOG_INFO("Audio input codec matches output codec, don't need to transcode.");
      } else {
        need_audio_transcode_ = true;
      }

      video_decoder_.initDecoder(input_video_codec_id_,
          (InitContextBeforeOpenCB)[this](AVCodecContext *context, AVDictionary *dict) {
        this->setupVideoDecodingParams(context, dict);
      });

      audio_decoder_.initDecoder(input_audio_codec_id_,
          (InitContextBeforeOpenCB)[this](AVCodecContext *context, AVDictionary *dict) {
        this->setupAudioDecodingParams(context, dict);
      });

      if (video_encoder_.initEncoder(output_video_codec_id,
          (InitContextBeforeOpenCB)[this](AVCodecContext *context, AVDictionary *dict) {
        this->setupVideoEncodingParams(context, dict);
      })) {
        if (video_encoder_.initializeStream(0, context_, &video_st_)) {
          context_->streams[0] = video_st_;
        }
      }

      if (audio_encoder_.initEncoder(output_audio_codec_id,
          (InitContextBeforeOpenCB)[this](AVCodecContext *context, AVDictionary *dict) {
        this->setupAudioEncodingParams(context, dict);
      })) {
        if (audio_encoder_.initializeStream(1, context_, &audio_st_)) {
          context_->streams[1] = audio_st_;
        }

        if (this->audioNeedsResample()) {
          ELOG_INFO("Encode Audio needs resampling.");
          need_audio_resample_ = true;
          audio_encoder_.initAudioResampler(audio_decoder_.codec_context_);
        }
      }

      if (!this->writeContextHeader()) {
        this->close();
        exit(1);
      }
      initialized_context_ = true;
    }
  }
  return initialized_context_;
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
    if (getAudioSinkSSRC() == 0) {
      ELOG_DEBUG("No audio detected");
      audio_map_ = RtpMap{0, "PCMU", 8000, AUDIO_TYPE, 1};
      input_audio_codec_id_ = AV_CODEC_ID_PCM_MULAW;
    }
  }
  if (need_to_send_fir_ && video_source_ssrc_) {
    sendFirPacket();
    need_to_send_fir_ = false;
  }

  if (type == VIDEO_PACKET) {
    RtpHeader* h = reinterpret_cast<RtpHeader*>(buffer);
    uint8_t payloadtype = h->getPayloadType();
    if (video_offset_ms_ == -1) {
      video_offset_ms_ = ClockUtils::durationToMs(clock::now() - first_data_received_);
      ELOG_DEBUG("File %s, video offset msec: %llu", context_->filename, video_offset_ms_);
      video_queue_.setTimebase(video_maps_[payloadtype].clock_rate);
    }

    // If this is a red header, let's push it to our fec_receiver_, which will spit out frames in one
    // of our other callbacks.
    // Otherwise, just stick it straight into the video queue.
    if (payloadtype == RED_90000_PT) {
      // The only things AddReceivedRedPacket uses are headerLength and sequenceNumber.
      // Unfortunately the amount of crap
      // we would have to pull in from the WebRtc project to fully construct
      // a webrtc::RTPHeader object is obscene.  So
      // let's just do this hacky fix.
      webrtc::RTPHeader hacky_header;
      hacky_header.headerLength = h->getHeaderLength();
      hacky_header.sequenceNumber = h->getSeqNumber();

      // AddReceivedRedPacket returns 0 if there's data to process
      if (0 == fec_receiver_->AddReceivedRedPacket(hacky_header, (const uint8_t*)buffer,
                                                   length, ULP_90000_PT)) {
        fec_receiver_->ProcessReceivedFec();
      }
    } else {
      video_queue_.pushPacket(buffer, length);
    }
  } else {
    if (audio_offset_ms_ == -1) {
      audio_offset_ms_ = ClockUtils::durationToMs(clock::now() - first_data_received_);
      ELOG_DEBUG("File %s, audio offset msec: %llu", context_->filename, audio_offset_ms_);

      // Let's also take a moment to set our audio queue timebase.
      RtpHeader* h = reinterpret_cast<RtpHeader*>(buffer);
      if (h->getPayloadType() == PCMU_8000_PT) {
        audio_queue_.setTimebase(8000);
      } else if (h->getPayloadType() == OPUS_48000_PT) {
        audio_queue_.setTimebase(48000);
      }
    }
    audio_queue_.pushPacket(buffer, length);
  }

  if (audio_queue_.hasData() || video_queue_.hasData()) {
    // One or both of our queues has enough data to write stuff out.  Notify our writer.
    cond_.notify_one();
  }
}

int ExternalOutput::sendFirPacket() {
    if (fb_sink_ != nullptr) {
      RtcpHeader pli_header;
      pli_header.setPacketType(RTCP_PS_Feedback_PT);
      pli_header.setBlockCount(1);
      pli_header.setSSRC(55543);
      pli_header.setSourceSSRC(video_source_ssrc_);
      pli_header.setLength(2);
      char *buf = reinterpret_cast<char*>(&pli_header);
      int len = (pli_header.getLength() + 1) * 4;
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
    while (audio_queue_.hasData()) {
      boost::shared_ptr<DataPacket> audio_packet = audio_queue_.popPacket();
      writeAudioData(audio_packet->data, audio_packet->length);
    }
    while (video_queue_.hasData()) {
      boost::shared_ptr<DataPacket> video_packet = video_queue_.popPacket();
      writeVideoData(video_packet->data, video_packet->length);
    }
  }

  // Since we're bailing, let's completely drain our queues of all data.
  while (audio_queue_.getSize() > 0) {
    boost::shared_ptr<DataPacket> audio_packet = audio_queue_.popPacket(true);  // ignore our minimum depth check
    writeAudioData(audio_packet->data, audio_packet->length);
  }
  while (video_queue_.getSize() > 0) {
    boost::shared_ptr<DataPacket> video_packet = video_queue_.popPacket(true);  // ignore our minimum depth check
    writeVideoData(video_packet->data, video_packet->length);
  }
}

bool ExternalOutput::writeContextHeader() {
  AVDictionary *opt = NULL;
  context_->oformat->flags |= AVFMT_VARIABLE_FPS;
  int ret = avformat_write_header(context_, &opt);
  av_dict_free(&opt);
  if (ret < 0) {
    static char error_str[255];
    av_strerror(ret, error_str, sizeof(error_str));
    ELOG_ERROR("Error writing header: %s", error_str);
    return false;
  }
  return true;
}

void ExternalOutput::setupAudioEncodingParams(AVCodecContext *context, AVDictionary *dict) {
  AVSampleFormat sample_fmt = context->codec->sample_fmts ?
      context->codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
  context->sample_fmt = sample_fmt;
  context->bit_rate = 64000;
  context->sample_rate = audio_encoder_.getBestSampleRate(context->codec);
  context->time_base = (AVRational) { 1, context->sample_rate };
  context->channel_layout = audio_encoder_.getChannelLayout(context->codec);
  context->channels = av_get_channel_layout_nb_channels(context->channel_layout);

  if (context->codec->id == AV_CODEC_ID_AAC) {
    context->sample_rate = 48000;
    context->time_base = (AVRational) { 1, context->sample_rate };
#ifdef FF_PROFILE_AAC_MAIN
    context->profile = FF_PROFILE_AAC_MAIN;
#endif
    av_dict_set(&dict, "strict", "experimental", 0);
  }
  if (context_->oformat->flags & AVFMT_GLOBALHEADER) {
    context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }
}

void ExternalOutput::setupAudioDecodingParams(AVCodecContext *context, AVDictionary *dict) {
  context->sample_rate = audio_map_.clock_rate;
  context->time_base = (AVRational) { 1, context->sample_rate };
  context->channels = audio_map_.channels;
  context->channel_layout = AV_CH_LAYOUT_STEREO;
}

void ExternalOutput::setupVideoEncodingParams(AVCodecContext *context, AVDictionary *dict) {
  context->gop_size = 45;
  context->bit_rate = 400 * 1000;
  context->width = 640;
  context->height = 480;
  context->framerate = (AVRational){25, 1};
  context->time_base = (AVRational){1, 25};
  context->pix_fmt = AV_PIX_FMT_YUV420P;

  if (context->codec->id == AV_CODEC_ID_H264) {
    av_opt_set(context->priv_data, "profile", "main", 0);
  }

  if (context->codec->id == AV_CODEC_ID_VP8 || context->codec->id == AV_CODEC_ID_VP9) {
    ELOG_DEBUG("Setting VPX params");
  }
  if (context_->oformat->flags & AVFMT_GLOBALHEADER) {
    context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }
}

void ExternalOutput::setupVideoDecodingParams(AVCodecContext *context, AVDictionary *dict)  {
  context->width = 640;
  context->height = 480;
  context->framerate = (AVRational){0, 1};
  context->pix_fmt = AV_PIX_FMT_YUV420P;
}

AVCodecID ExternalOutput::bestMatchOutputCodecId(AVCodecID input_codec_id) {
  AVMediaType type = avcodec_get_type(input_codec_id);
  if (av_codec_get_tag(context_->oformat->codec_tag, input_codec_id) != 0) {
    return input_codec_id;
  } else {
    if (type == AVMEDIA_TYPE_VIDEO) {
      return context_->oformat->video_codec;
    } else if (type == AVMEDIA_TYPE_AUDIO) {
      return context_->oformat->audio_codec;
    } else {
      return AV_CODEC_ID_NONE;
    }
  }
}

bool ExternalOutput::audioNeedsResample() {
  if (AV_SAMPLE_FMT_S16 != audio_encoder_.codec_context_->sample_fmt) {
    return true;
  }
  if (audio_decoder_.codec_context_->sample_rate != audio_encoder_.codec_context_->sample_rate) {
    return true;
  }
  if (audio_decoder_.codec_context_->channels != audio_encoder_.codec_context_->channels) {
    return true;
  }
  if (audio_decoder_.codec_context_->channel_layout != audio_encoder_.codec_context_->channel_layout) {
    return true;
  }
  return false;
}

AVDictionary* ExternalOutput::genVideoMetadata() {
    AVDictionary* dict = NULL;
    switch (ext_processor_.getVideoRotation()) {
      case kVideoRotation_0:
        av_dict_set(&dict, "rotate", "0", 0);
        break;
      case kVideoRotation_90:
        av_dict_set(&dict, "rotate", "90", 0);
        break;
      case kVideoRotation_180:
        av_dict_set(&dict, "rotate", "180", 0);
        break;
      case kVideoRotation_270:
        av_dict_set(&dict, "rotate", "270", 0);
        break;
      default:
        av_dict_set(&dict, "rotate", "0", 0);
        break;
    }
    return dict;
}

}  // namespace erizo
