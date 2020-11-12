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
    inited_{false}, video_stream_{nullptr},
    audio_stream_{nullptr}, video_source_ssrc_{0},
    first_video_timestamp_{-1}, first_audio_timestamp_{-1},
    first_data_received_{}, video_offset_ms_{-1}, audio_offset_ms_{-1},
    need_to_send_fir_{true}, rtp_mappings_{rtp_mappings}, video_codec_{AV_CODEC_ID_NONE},
    audio_codec_{AV_CODEC_ID_NONE}, pipeline_initialized_{false}, ext_processor_{ext_mappings} {
  ELOG_DEBUG("Creating output to %s", output_url.c_str());

  // TODO(pedro): these should really only be called once per application run
  av_register_all();
  avcodec_register_all();

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

    context_->oformat = av_guess_format(nullptr,  context_->filename, nullptr);
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
  sink_fb_source_ = std::dynamic_pointer_cast<FeedbackSource>(shared_from_this());
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

boost::future<void> ExternalOutput::close() {
  std::shared_ptr<ExternalOutput> shared_this = shared_from_this();
  return asyncTask([shared_this] (std::shared_ptr<ExternalOutput> connection) {
    shared_this->syncClose();
  });
}

void ExternalOutput::syncClose() {
  if (!recording_) {
    return;
  }
  // Stop our thread so we can safely nuke libav stuff and close our
  // our file.
  cond_.notify_one();
  thread_.join();

  if (audio_stream_ != nullptr && video_stream_ != nullptr && context_ != nullptr) {
      av_write_trailer(context_);
  }

  if (video_stream_ && video_stream_->codec != nullptr) {
      avcodec_close(video_stream_->codec);
  }

  if (audio_stream_ && audio_stream_->codec != nullptr) {
      avcodec_close(audio_stream_->codec);
  }

  if (context_ != nullptr) {
      avio_close(context_->pb);
      avformat_free_context(context_);
      context_ = nullptr;
  }

  pipeline_initialized_ = false;
  recording_ = false;

  ELOG_DEBUG("Closed Successfully");
}
boost::future<void> ExternalOutput::asyncTask(
    std::function<void(std::shared_ptr<ExternalOutput>)> f) {
  auto task_promise = std::make_shared<boost::promise<void>>();
  std::weak_ptr<ExternalOutput> weak_this = shared_from_this();
  worker_->task([weak_this, f, task_promise] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
    task_promise->set_value();
  });
  return task_promise->get_future();
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

  initContext();

  if (audio_stream_ == nullptr) {
      // not yet.
      return;
  }

  long long current_timestamp = head->getTimestamp();  // NOLINT
  if (current_timestamp - first_audio_timestamp_ < 0) {
      // we wrapped.  add 2^32 to correct this. We only handle a single wrap around
      // since that's 13 hours of recording, minimum.
      current_timestamp += 0xFFFFFFFF;
  }

  long long timestamp_to_write = (current_timestamp - first_audio_timestamp_) /  // NOLINT
                                    (audio_stream_->codec->sample_rate / audio_stream_->time_base.den);
  // generally 48000 / 1000 for the denominator portion, at least for opus
  // Adjust for our start time offset

  // in practice, our timebase den is 1000, so this operation is a no-op.
  timestamp_to_write += audio_offset_ms_ / (1000 / audio_stream_->time_base.den);

  AVPacket av_packet;
  av_init_packet(&av_packet);
  av_packet.data = reinterpret_cast<uint8_t*>(buf) + head->getHeaderLength();
  av_packet.size = len - head->getHeaderLength();
  av_packet.pts = timestamp_to_write;
  av_packet.stream_index = 1;
  av_interleaved_write_frame(context_, &av_packet);   // takes ownership of the packet
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
  if (audio_codec_ != AV_CODEC_ID_NONE) {
    return;
  }
  audio_map_ = map;
  if (map.encoding_name == "opus") {
    audio_codec_ = AV_CODEC_ID_OPUS;
  } else if (map.encoding_name == "PCMU") {
    audio_codec_ = AV_CODEC_ID_PCM_MULAW;
  }
}

void ExternalOutput::updateVideoCodec(RtpMap map) {
  if (video_codec_ != AV_CODEC_ID_NONE) {
    return;
  }
  video_map_ = map;
  if (map.encoding_name == "VP8") {
    depacketizer_.reset(new Vp8Depacketizer());
    video_codec_ = AV_CODEC_ID_VP8;
  } else if (map.encoding_name == "H264") {
    depacketizer_.reset(new H264Depacketizer());
    video_codec_ = AV_CODEC_ID_H264;
  }
}

void ExternalOutput::maybeWriteVideoPacket(char* buf, int len) {
  RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);
  depacketizer_->fetchPacket((unsigned char*)buf, len);
  bool deliver = depacketizer_->processPacket();

  initContext();
  if (video_stream_ == nullptr) {
    // could not init our context yet.
    return;
  }

  if (deliver) {
    long long current_timestamp = head->getTimestamp();  // NOLINT
    if (current_timestamp - first_video_timestamp_ < 0) {
      // we wrapped.  add 2^32 to correct this.
      // We only handle a single wrap around since that's ~13 hours of recording, minimum.
      current_timestamp += 0xFFFFFFFF;
    }

    // All of our video offerings are using a 90khz clock.
    long long timestamp_to_write = (current_timestamp - first_video_timestamp_) /  // NOLINT
                                              (video_map_.clock_rate / video_stream_->time_base.den);

    // Adjust for our start time offset

    // in practice, our timebase den is 1000, so this operation is a no-op.
    timestamp_to_write += video_offset_ms_ / (1000 / video_stream_->time_base.den);

    AVPacket av_packet;
    av_init_packet(&av_packet);
    av_packet.data = depacketizer_->frame();
    av_packet.size = depacketizer_->frameSize();
    av_packet.pts = timestamp_to_write;
    av_packet.stream_index = 0;
    av_interleaved_write_frame(context_, &av_packet);   // takes ownership of the packet
    depacketizer_->reset();
  }
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
  if (video_codec_ != AV_CODEC_ID_NONE &&
            audio_codec_ != AV_CODEC_ID_NONE &&
            video_stream_ == nullptr &&
            audio_stream_ == nullptr) {
    AVCodec* video_codec = avcodec_find_encoder(video_codec_);
    if (video_codec == nullptr) {
      ELOG_ERROR("Could not find video codec");
      return false;
    }
    need_to_send_fir_ = true;
    video_queue_.setTimebase(video_map_.clock_rate);
    video_stream_ = avformat_new_stream(context_, video_codec);
    video_stream_->id = 0;
    video_stream_->codec->codec_id = video_codec_;
    video_stream_->codec->width = 640;
    video_stream_->codec->height = 480;
    video_stream_->time_base = (AVRational) { 1, 30 };
    video_stream_->metadata = genVideoMetadata();
    // A decent guess here suffices; if processing the file with ffmpeg,
    // use -vsync 0 to force it not to duplicate frames.
    video_stream_->codec->pix_fmt = PIX_FMT_YUV420P;
    if (context_->oformat->flags & AVFMT_GLOBALHEADER) {
      video_stream_->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    context_->oformat->flags |= AVFMT_VARIABLE_FPS;

    AVCodec* audio_codec = avcodec_find_encoder(audio_codec_);
    if (audio_codec == nullptr) {
      ELOG_ERROR("Could not find audio codec");
      return false;
    }

    audio_stream_ = avformat_new_stream(context_, audio_codec);
    audio_stream_->id = 1;
    audio_stream_->codec->codec_id = audio_codec_;
    audio_stream_->codec->sample_rate = audio_map_.clock_rate;
    audio_stream_->time_base = (AVRational) { 1, audio_stream_->codec->sample_rate };
    audio_stream_->codec->channels = audio_map_.channels;
    if (context_->oformat->flags & AVFMT_GLOBALHEADER) {
      audio_stream_->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    context_->streams[0] = video_stream_;
    context_->streams[1] = audio_stream_;
    if (avio_open(&context_->pb, context_->filename, AVIO_FLAG_WRITE) < 0) {
      ELOG_ERROR("Error opening output file");
      return false;
    }

    if (avformat_write_header(context_, nullptr) < 0) {
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
    if (getAudioSinkSSRC() == 0) {
      ELOG_DEBUG("No audio detected");
      audio_map_ = RtpMap{0, "PCMU", 8000, AUDIO_TYPE, 1};
      audio_codec_ = AV_CODEC_ID_PCM_MULAW;
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
    if (auto fb_sink = fb_sink_.lock()) {
      RtcpHeader pli_header;
      pli_header.setPacketType(RTCP_PS_Feedback_PT);
      pli_header.setBlockCount(1);
      pli_header.setSSRC(55543);
      pli_header.setSourceSSRC(video_source_ssrc_);
      pli_header.setLength(2);
      char *buf = reinterpret_cast<char*>(&pli_header);
      int len = (pli_header.getLength() + 1) * 4;
      std::shared_ptr<DataPacket> pli_packet = std::make_shared<DataPacket>(0, buf, len, VIDEO_PACKET);
      fb_sink->deliverFeedback(pli_packet);
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
    if (!inited_ && first_data_received_ != time_point()) {
      inited_ = true;
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
