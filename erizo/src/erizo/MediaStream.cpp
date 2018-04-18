
/*
 * MediaStream.cpp
 */

#include <cstdio>
#include <map>
#include <algorithm>
#include <string>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <ctime>

#include "./MediaStream.h"
#include "./SdpInfo.h"
#include"./WebRtcConnection.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtcpAggregator.h"
#include "rtp/RtcpForwarder.h"
#include "rtp/RtpSlideShowHandler.h"
#include "rtp/RtpTrackMuteHandler.h"
#include "rtp/BandwidthEstimationHandler.h"
#include "rtp/FecReceiverHandler.h"
#include "rtp/RtcpProcessorHandler.h"
#include "rtp/RtpRetransmissionHandler.h"
#include "rtp/RtcpFeedbackGenerationHandler.h"
#include "rtp/RtpPaddingRemovalHandler.h"
#include "rtp/StatsHandler.h"
#include "rtp/SRPacketHandler.h"
#include "rtp/SenderBandwidthEstimationHandler.h"
#include "rtp/LayerDetectorHandler.h"
#include "rtp/LayerBitrateCalculationHandler.h"
#include "rtp/QualityFilterHandler.h"
#include "rtp/QualityManager.h"
#include "rtp/PliPacerHandler.h"
#include "rtp/RtpPaddingGeneratorHandler.h"
#include "rtp/RtpUtils.h"
#include "rtp/PacketCodecParser.h"

namespace erizo {
DEFINE_LOGGER(MediaStream, "MediaStream");

MediaStream::MediaStream(std::shared_ptr<Worker> worker,
  std::shared_ptr<WebRtcConnection> connection,
  const std::string& media_stream_id,
  const std::string& media_stream_label,
  bool is_publisher) :
    audio_enabled_{false}, video_enabled_{false},
    connection_{connection},
    stream_id_{media_stream_id},
    mslabel_ {media_stream_label},
    bundle_{false},
    pipeline_{Pipeline::create()},
    worker_{worker},
    audio_muted_{false}, video_muted_{false},
    pipeline_initialized_{false},
    is_publisher_{is_publisher} {
  setVideoSinkSSRC(kDefaultVideoSinkSSRC);
  setAudioSinkSSRC(kDefaultAudioSinkSSRC);
  ELOG_INFO("%s message: constructor, id: %s",
      toLog(), media_stream_id.c_str());
  source_fb_sink_ = this;
  sink_fb_source_ = this;
  stats_ = std::make_shared<Stats>();
  quality_manager_ = std::make_shared<QualityManager>();
  packet_buffer_ = std::make_shared<PacketBufferService>();
  std::srand(std::time(nullptr));
  audio_sink_ssrc_ = std::rand();
  video_sink_ssrc_ = std::rand();

  rtcp_processor_ = std::make_shared<RtcpForwarder>(static_cast<MediaSink*>(this), static_cast<MediaSource*>(this));

  should_send_feedback_ = true;
  slide_show_mode_ = false;

  mark_ = clock::now();

  rate_control_ = 0;
  sending_ = true;
}

MediaStream::~MediaStream() {
  ELOG_DEBUG("%s message:Destructor called", toLog());
  ELOG_DEBUG("%s message: Destructor ended", toLog());
}

uint32_t MediaStream::getMaxVideoBW() {
  uint32_t bitrate = rtcp_processor_ ? rtcp_processor_->getMaxVideoBW() : 0;
  return bitrate;
}

void MediaStream::setMaxVideoBW(uint32_t max_video_bw) {
  asyncTask([max_video_bw] (std::shared_ptr<MediaStream> stream) {
    if (stream->rtcp_processor_) {
      stream->rtcp_processor_->setMaxVideoBW(max_video_bw * 1000);
      stream->pipeline_->notifyUpdate();
    }
  });
}

void MediaStream::syncClose() {
  ELOG_DEBUG("%s message:Close called", toLog());
  if (!sending_) {
    return;
  }
  sending_ = false;
  video_sink_ = nullptr;
  audio_sink_ = nullptr;
  fb_sink_ = nullptr;
  pipeline_initialized_ = false;
  pipeline_->close();
  pipeline_.reset();
  connection_.reset();
  ELOG_DEBUG("%s message: Close ended", toLog());
}
void MediaStream::close() {
  ELOG_DEBUG("%s message: Async close called", toLog());
  std::shared_ptr<MediaStream> shared_this = shared_from_this();
  asyncTask([shared_this] (std::shared_ptr<MediaStream> stream) {
    shared_this->syncClose();
  });
}

bool MediaStream::init() {
  return true;
}

bool MediaStream::isSourceSSRC(uint32_t ssrc) {
  return isVideoSourceSSRC(ssrc) || isAudioSourceSSRC(ssrc);
}

bool MediaStream::isSinkSSRC(uint32_t ssrc) {
  return isVideoSinkSSRC(ssrc) || isAudioSinkSSRC(ssrc);
}

bool MediaStream::setRemoteSdp(std::shared_ptr<SdpInfo> sdp) {
  ELOG_DEBUG("%s message: setting remote SDP", toLog());
  if (!sending_) {
    return true;
  }
  remote_sdp_ = sdp;
  if (remote_sdp_->videoBandwidth != 0) {
    ELOG_DEBUG("%s message: Setting remote BW, maxVideoBW: %u", toLog(), remote_sdp_->videoBandwidth);
    this->rtcp_processor_->setMaxVideoBW(remote_sdp_->videoBandwidth*1000);
  }

  if (pipeline_initialized_) {
    pipeline_->notifyUpdate();
    return true;
  }


  bundle_ = remote_sdp_->isBundle;
  auto video_ssrc_list_it = remote_sdp_->video_ssrc_map.find(getLabel());
  if (video_ssrc_list_it != remote_sdp_->video_ssrc_map.end()) {
    setVideoSourceSSRCList(video_ssrc_list_it->second);
  }

  auto audio_ssrc_it = remote_sdp_->audio_ssrc_map.find(getLabel());
  if (audio_ssrc_it != remote_sdp_->audio_ssrc_map.end()) {
    setAudioSourceSSRC(audio_ssrc_it->second);
  }

  if (getVideoSourceSSRCList().empty() ||
      (getVideoSourceSSRCList().size() == 1 && getVideoSourceSSRCList()[0] == 0)) {
    std::vector<uint32_t> default_ssrc_list;
    default_ssrc_list.push_back(kDefaultVideoSinkSSRC);
    setVideoSourceSSRCList(default_ssrc_list);
  }

  if (getAudioSourceSSRC() == 0) {
    setAudioSourceSSRC(kDefaultAudioSinkSSRC);
  }

  audio_enabled_ = remote_sdp_->hasAudio;
  video_enabled_ = remote_sdp_->hasVideo;

  rtcp_processor_->addSourceSsrc(getAudioSourceSSRC());
  std::for_each(video_source_ssrc_list_.begin(), video_source_ssrc_list_.end(), [this] (uint32_t new_ssrc){
      rtcp_processor_->addSourceSsrc(new_ssrc);
  });

  initializePipeline();

  return true;
}

bool MediaStream::setLocalSdp(std::shared_ptr<SdpInfo> sdp) {
  local_sdp_ = sdp;
  return true;
}

void MediaStream::initializePipeline() {
  handler_manager_ = std::make_shared<HandlerManager>(shared_from_this());
  pipeline_->addService(shared_from_this());
  pipeline_->addService(handler_manager_);
  pipeline_->addService(rtcp_processor_);
  pipeline_->addService(stats_);
  pipeline_->addService(quality_manager_);
  pipeline_->addService(packet_buffer_);

  pipeline_->addFront(PacketReader(this));

  pipeline_->addFront(RtcpProcessorHandler());
  pipeline_->addFront(FecReceiverHandler());
  pipeline_->addFront(LayerBitrateCalculationHandler());
  pipeline_->addFront(QualityFilterHandler());
  pipeline_->addFront(IncomingStatsHandler());
  pipeline_->addFront(RtpTrackMuteHandler());
  pipeline_->addFront(RtpSlideShowHandler());
  pipeline_->addFront(RtpPaddingGeneratorHandler());
  pipeline_->addFront(PliPacerHandler());
  pipeline_->addFront(BandwidthEstimationHandler());
  pipeline_->addFront(RtpPaddingRemovalHandler());
  pipeline_->addFront(RtcpFeedbackGenerationHandler());
  pipeline_->addFront(RtpRetransmissionHandler());
  pipeline_->addFront(SRPacketHandler());
  pipeline_->addFront(SenderBandwidthEstimationHandler());
  pipeline_->addFront(LayerDetectorHandler());
  pipeline_->addFront(OutgoingStatsHandler());
  pipeline_->addFront(PacketCodecParser());

  pipeline_->addFront(PacketWriter(this));
  pipeline_->finalize();
  pipeline_initialized_ = true;
}

int MediaStream::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  if (audio_enabled_ == true) {
    sendPacketAsync(std::make_shared<DataPacket>(*audio_packet));
  }
  return audio_packet->length;
}

int MediaStream::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  if (video_enabled_ == true) {
    sendPacketAsync(std::make_shared<DataPacket>(*video_packet));
  }
  return video_packet->length;
}

int MediaStream::deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(fb_packet->data);
  uint32_t recvSSRC = chead->getSourceSSRC();
  if (chead->isREMB()) {
    for (uint8_t index = 0; index < chead->getREMBNumSSRC(); index++) {
      uint32_t ssrc = chead->getREMBFeedSSRC(index);
      if (isVideoSourceSSRC(ssrc)) {
        recvSSRC = ssrc;
        break;
      }
    }
  }
  if (isVideoSourceSSRC(recvSSRC)) {
    fb_packet->type = VIDEO_PACKET;
    sendPacketAsync(fb_packet);
  } else if (isAudioSourceSSRC(recvSSRC)) {
    fb_packet->type = AUDIO_PACKET;
    sendPacketAsync(fb_packet);
  } else {
    ELOG_DEBUG("%s deliverFeedback unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
  }
  return fb_packet->length;
}

int MediaStream::deliverEvent_(MediaEventPtr event) {
  auto stream_ptr = shared_from_this();
  worker_->task([stream_ptr, event]{
    if (!stream_ptr->pipeline_initialized_) {
      return;
    }
    stream_ptr->pipeline_->notifyEvent(event);
  });
  return 1;
}

void MediaStream::onTransportData(std::shared_ptr<DataPacket> incoming_packet, Transport *transport) {
  if ((audio_sink_ == nullptr && video_sink_ == nullptr && fb_sink_ == nullptr)) {
    return;
  }

  std::shared_ptr<DataPacket> packet = std::make_shared<DataPacket>(*incoming_packet);

  if (transport->mediaType == AUDIO_TYPE) {
    packet->type = AUDIO_PACKET;
  } else if (transport->mediaType == VIDEO_TYPE) {
    packet->type = VIDEO_PACKET;
  }
  auto stream_ptr = shared_from_this();

  worker_->task([stream_ptr, packet]{
    if (!stream_ptr->pipeline_initialized_) {
      ELOG_DEBUG("%s message: Pipeline not initialized yet.", stream_ptr->toLog());
      return;
    }

    char* buf = packet->data;
    RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
    if (!chead->isRtcp()) {
      uint32_t recvSSRC = head->getSSRC();
      if (stream_ptr->isVideoSourceSSRC(recvSSRC)) {
        packet->type = VIDEO_PACKET;
      } else if (stream_ptr->isAudioSourceSSRC(recvSSRC)) {
        packet->type = AUDIO_PACKET;
      }
    }

    stream_ptr->pipeline_->read(std::move(packet));
  });
}

void MediaStream::read(std::shared_ptr<DataPacket> packet) {
  char* buf = packet->data;
  int len = packet->length;
  // PROCESS RTCP
  RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
  uint32_t recvSSRC = 0;
  if (!chead->isRtcp()) {
    recvSSRC = head->getSSRC();
  } else if (chead->packettype == RTCP_Sender_PT) {  // Sender Report
    recvSSRC = chead->getSSRC();
  }
  // DELIVER FEEDBACK (RR, FEEDBACK PACKETS)
  if (chead->isFeedback()) {
    if (fb_sink_ != nullptr && should_send_feedback_) {
      fb_sink_->deliverFeedback(std::move(packet));
    }
  } else {
    // RTP or RTCP Sender Report
    if (bundle_) {
      // Check incoming SSRC
      // Deliver data
      if (isVideoSourceSSRC(recvSSRC) && video_sink_) {
        parseIncomingPayloadType(buf, len, VIDEO_PACKET);
        video_sink_->deliverVideoData(std::move(packet));
      } else if (isAudioSourceSSRC(recvSSRC) && audio_sink_) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        audio_sink_->deliverAudioData(std::move(packet));
      } else {
        ELOG_DEBUG("%s read video unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                    toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
      }
    } else {
      if (packet->type == AUDIO_PACKET && audio_sink_) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        // Firefox does not send SSRC in SDP
        if (getAudioSourceSSRC() == 0) {
          ELOG_DEBUG("%s discoveredAudioSourceSSRC:%u", toLog(), recvSSRC);
          this->setAudioSourceSSRC(recvSSRC);
        }
        audio_sink_->deliverAudioData(std::move(packet));
      } else if (packet->type == VIDEO_PACKET && video_sink_) {
        parseIncomingPayloadType(buf, len, VIDEO_PACKET);
        // Firefox does not send SSRC in SDP
        if (getVideoSourceSSRC() == 0) {
          ELOG_DEBUG("%s discoveredVideoSourceSSRC:%u", toLog(), recvSSRC);
          this->setVideoSourceSSRC(recvSSRC);
        }
        // change ssrc for RTP packets, don't touch here if RTCP
        video_sink_->deliverVideoData(std::move(packet));
      }
    }  // if not bundle
  }  // if not Feedback
}

void MediaStream::setMediaStreamEventListener(MediaStreamEventListener* listener) {
  boost::mutex::scoped_lock lock(event_listener_mutex_);
  this->media_stream_event_listener_ = listener;
}

void MediaStream::notifyMediaStreamEvent(const std::string& type, const std::string& message) {
  boost::mutex::scoped_lock lock(event_listener_mutex_);
  if (this->media_stream_event_listener_ != nullptr) {
    media_stream_event_listener_->notifyMediaStreamEvent(type, message);
  }
}

void MediaStream::notifyToEventSink(MediaEventPtr event) {
  event_sink_->deliverEvent(event);
}

int MediaStream::sendPLI() {
  RtcpHeader thePLI;
  thePLI.setPacketType(RTCP_PS_Feedback_PT);
  thePLI.setBlockCount(1);
  thePLI.setSSRC(this->getVideoSinkSSRC());
  thePLI.setSourceSSRC(this->getVideoSourceSSRC());
  thePLI.setLength(2);
  char *buf = reinterpret_cast<char*>(&thePLI);
  int len = (thePLI.getLength() + 1) * 4;
  sendPacketAsync(std::make_shared<DataPacket>(0, buf, len, VIDEO_PACKET));
  return len;
}

void MediaStream::sendPLIToFeedback() {
  if (fb_sink_) {
    fb_sink_->deliverFeedback(RtpUtils::createPLI(this->getVideoSinkSSRC(),
      this->getVideoSourceSSRC()));
  }
}
// changes the outgoing payload type for in the given data packet
void MediaStream::sendPacketAsync(std::shared_ptr<DataPacket> packet) {
  if (!sending_) {
    return;
  }
  auto stream_ptr = shared_from_this();
  if (packet->comp == -1) {
    sending_ = false;
    auto p = std::make_shared<DataPacket>();
    p->comp = -1;
    worker_->task([stream_ptr, p]{
      stream_ptr->sendPacket(p);
    });
    return;
  }

  changeDeliverPayloadType(packet.get(), packet->type);
  worker_->task([stream_ptr, packet]{
    stream_ptr->sendPacket(packet);
  });
}

void MediaStream::setSlideShowMode(bool state) {
  ELOG_DEBUG("%s slideShowMode: %u", toLog(), state);
  if (slide_show_mode_ == state) {
    return;
  }
  asyncTask([state] (std::shared_ptr<MediaStream> media_stream) {
    media_stream->stats_->getNode()[media_stream->getVideoSinkSSRC()].insertStat(
      "erizoSlideShow",
       CumulativeStat{state});
  });
  slide_show_mode_ = state;
  notifyUpdateToHandlers();
}

void MediaStream::muteStream(bool mute_video, bool mute_audio) {
  asyncTask([mute_audio, mute_video] (std::shared_ptr<MediaStream> media_stream) {
    ELOG_DEBUG("%s message: muteStream, mute_video: %u, mute_audio: %u", media_stream->toLog(), mute_video, mute_audio);
    media_stream->audio_muted_ = mute_audio;
    media_stream->video_muted_ = mute_video;
    media_stream->stats_->getNode()[media_stream->getAudioSinkSSRC()].insertStat("erizoAudioMute",
                                                                             CumulativeStat{mute_audio});
    media_stream->stats_->getNode()[media_stream->getAudioSinkSSRC()].insertStat("erizoVideoMute",
                                                                             CumulativeStat{mute_video});
    media_stream->pipeline_->notifyUpdate();
  });
}

void MediaStream::setVideoConstraints(int max_video_width, int max_video_height, int max_video_frame_rate) {
  asyncTask([max_video_width, max_video_height, max_video_frame_rate] (std::shared_ptr<MediaStream> media_stream) {
    media_stream->quality_manager_->setVideoConstraints(max_video_width, max_video_height, max_video_frame_rate);
  });
}

void MediaStream::setTransportInfo(std::string audio_info, std::string video_info) {
  if (video_enabled_) {
    uint32_t video_sink_ssrc = getVideoSinkSSRC();
    uint32_t video_source_ssrc = getVideoSourceSSRC();

    if (video_sink_ssrc != kDefaultVideoSinkSSRC) {
      stats_->getNode()[video_sink_ssrc].insertStat("clientHostType", StringStat{video_info});
    }
    if (video_source_ssrc != 0) {
      stats_->getNode()[video_source_ssrc].insertStat("clientHostType", StringStat{video_info});
    }
  }

  if (audio_enabled_) {
    uint32_t audio_sink_ssrc = getAudioSinkSSRC();
    uint32_t audio_source_ssrc = getAudioSourceSSRC();

    if (audio_sink_ssrc != kDefaultAudioSinkSSRC) {
      stats_->getNode()[audio_sink_ssrc].insertStat("clientHostType", StringStat{audio_info});
    }
    if (audio_source_ssrc != 0) {
      stats_->getNode()[audio_source_ssrc].insertStat("clientHostType", StringStat{audio_info});
    }
  }
}

void MediaStream::setFeedbackReports(bool will_send_fb, uint32_t target_bitrate) {
  if (slide_show_mode_) {
    target_bitrate = 0;
  }

  this->should_send_feedback_ = will_send_fb;
  if (target_bitrate == 1) {
    this->video_enabled_ = false;
  }
  this->rate_control_ = target_bitrate;
}

void MediaStream::setMetadata(std::map<std::string, std::string> metadata) {
  setLogContext(metadata);
}

WebRTCEvent MediaStream::getCurrentState() {
  return connection_->getCurrentState();
}

void MediaStream::getJSONStats(std::function<void(std::string)> callback) {
  asyncTask([callback] (std::shared_ptr<MediaStream> stream) {
    std::string requested_stats = stream->stats_->getStats();
    //  ELOG_DEBUG("%s message: Stats, stats: %s", stream->toLog(), requested_stats.c_str());
    callback(requested_stats);
  });
}

void MediaStream::changeDeliverPayloadType(DataPacket *dp, packetType type) {
  RtpHeader* h = reinterpret_cast<RtpHeader*>(dp->data);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(dp->data);
  if (!chead->isRtcp()) {
      int internalPT = h->getPayloadType();
      int externalPT = internalPT;
      if (type == AUDIO_PACKET) {
          externalPT = remote_sdp_->getAudioExternalPT(internalPT);
      } else if (type == VIDEO_PACKET) {
          externalPT = remote_sdp_->getVideoExternalPT(externalPT);
      }
      if (internalPT != externalPT) {
          h->setPayloadType(externalPT);
      }
  }
}

// parses incoming payload type, replaces occurence in buf
void MediaStream::parseIncomingPayloadType(char *buf, int len, packetType type) {
  RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(buf);
  RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
  if (!chead->isRtcp()) {
    int externalPT = h->getPayloadType();
    int internalPT = externalPT;
    if (type == AUDIO_PACKET) {
      internalPT = remote_sdp_->getAudioInternalPT(externalPT);
    } else if (type == VIDEO_PACKET) {
      internalPT = remote_sdp_->getVideoInternalPT(externalPT);
    }
    if (externalPT != internalPT) {
      h->setPayloadType(internalPT);
    } else {
//        ELOG_WARN("onTransportData did not find mapping for %i", externalPT);
    }
  }
}

void MediaStream::write(std::shared_ptr<DataPacket> packet) {
  if (connection_) {
    connection_->write(packet);
  }
}

void MediaStream::enableHandler(const std::string &name) {
  asyncTask([name] (std::shared_ptr<MediaStream> conn) {
    conn->pipeline_->enable(name);
  });
}

void MediaStream::disableHandler(const std::string &name) {
  asyncTask([name] (std::shared_ptr<MediaStream> conn) {
    conn->pipeline_->disable(name);
  });
}

void MediaStream::notifyUpdateToHandlers() {
  asyncTask([] (std::shared_ptr<MediaStream> conn) {
    conn->pipeline_->notifyUpdate();
  });
}

void MediaStream::asyncTask(std::function<void(std::shared_ptr<MediaStream>)> f) {
  std::weak_ptr<MediaStream> weak_this = shared_from_this();
  worker_->task([weak_this, f] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
  });
}

void MediaStream::sendPacket(std::shared_ptr<DataPacket> p) {
  if (!sending_) {
    return;
  }
  uint32_t partial_bitrate = 0;
  uint64_t sentVideoBytes = 0;
  uint64_t lastSecondVideoBytes = 0;

  if (rate_control_ && !slide_show_mode_) {
    if (p->type == VIDEO_PACKET) {
      if (rate_control_ == 1) {
        return;
      }
      now_ = clock::now();
      if ((now_ - mark_) >= kBitrateControlPeriod) {
        mark_ = now_;
        lastSecondVideoBytes = sentVideoBytes;
      }
      partial_bitrate = ((sentVideoBytes - lastSecondVideoBytes) * 8) * 10;
      if (partial_bitrate > this->rate_control_) {
        return;
      }
      sentVideoBytes += p->length;
    }
  }
  if (!pipeline_initialized_) {
    ELOG_DEBUG("%s message: Pipeline not initialized yet.", toLog());
    return;
  }

  pipeline_->write(std::move(p));
}

void MediaStream::setQualityLayer(int spatial_layer, int temporal_layer) {
  asyncTask([spatial_layer, temporal_layer] (std::shared_ptr<MediaStream> media_stream) {
    media_stream->quality_manager_->forceLayers(spatial_layer, temporal_layer);
  });
}

void MediaStream::setMinDesiredSpatialLayer(int spatial_layer) {
  asyncTask([spatial_layer] (std::shared_ptr<MediaStream> media_stream) {
    media_stream->quality_manager_->setMinDesiredSpatialLayer(spatial_layer);
  });
}

}  // namespace erizo
