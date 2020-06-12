
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
#include "rtp/FakeKeyframeGeneratorHandler.h"
#include "rtp/StatsHandler.h"
#include "rtp/SRPacketHandler.h"
#include "rtp/LayerDetectorHandler.h"
#include "rtp/LayerBitrateCalculationHandler.h"
#include "rtp/QualityFilterHandler.h"
#include "rtp/QualityManager.h"
#include "rtp/PeriodicPliHandler.h"
#include "rtp/PliPriorityHandler.h"
#include "rtp/PliPacerHandler.h"
#include "rtp/RtpPaddingGeneratorHandler.h"
#include "rtp/RtpUtils.h"
#include "rtp/PacketCodecParser.h"

namespace erizo {
DEFINE_LOGGER(MediaStream, "MediaStream");
log4cxx::LoggerPtr MediaStream::statsLogger = log4cxx::Logger::getLogger("StreamStats");

static constexpr auto kStreamStatsPeriod = std::chrono::seconds(120);
static constexpr uint64_t kInitialBitrate = 300000;

MediaStream::MediaStream(std::shared_ptr<Worker> worker,
  std::shared_ptr<WebRtcConnection> connection,
  const std::string& media_stream_id,
  const std::string& media_stream_label,
  bool is_publisher,
  int session_version) :
    audio_enabled_{false}, video_enabled_{false},
    media_stream_event_listener_{nullptr},
    connection_{std::move(connection)},
    stream_id_{media_stream_id},
    mslabel_ {media_stream_label},
    bundle_{false},
    pipeline_{Pipeline::create()},
    worker_{std::move(worker)},
    audio_muted_{false}, video_muted_{false},
    pipeline_initialized_{false},
    is_publisher_{is_publisher},
    simulcast_{false},
    bitrate_from_max_quality_layer_{0},
    video_bitrate_{0},
    random_generator_{random_device_()},
    target_padding_bitrate_{0},
    periodic_keyframes_requested_{false},
    periodic_keyframe_interval_{0},
    session_version_{session_version} {
  if (is_publisher) {
    setVideoSinkSSRC(kDefaultVideoSinkSSRC);
    setAudioSinkSSRC(kDefaultAudioSinkSSRC);
  } else {
    setAudioSinkSSRC(1000000000 + getRandomValue(0, 999999999));
    setVideoSinkSSRC(1000000000 + getRandomValue(0, 999999999));
  }
  ELOG_INFO("%s message: constructor, id: %s",
      toLog(), media_stream_id.c_str());
  stats_ = std::make_shared<Stats>();
  log_stats_ = std::make_shared<Stats>();
  quality_manager_ = std::make_shared<QualityManager>();
  packet_buffer_ = std::make_shared<PacketBufferService>();

  rtcp_processor_ = std::make_shared<RtcpForwarder>(static_cast<MediaSink*>(this), static_cast<MediaSource*>(this));

  should_send_feedback_ = true;
  slide_show_mode_ = false;

  mark_ = clock::now();

  rate_control_ = 0;
  sending_ = true;
  ready_ = false;
}

MediaStream::~MediaStream() {
  ELOG_DEBUG("%s message:Destructor called", toLog());
  ELOG_DEBUG("%s message: Destructor ended", toLog());
}

uint32_t MediaStream::getRandomValue(uint32_t min, uint32_t max) {
  std::uniform_int_distribution<> distr(min, max);
  return std::round(distr(random_generator_));
}

uint32_t MediaStream::getMaxVideoBW() {
  uint32_t bitrate = rtcp_processor_ ? rtcp_processor_->getMaxVideoBW() : 0;
  return bitrate;
}

void MediaStream::setMaxVideoBW(uint32_t max_video_bw) {
  asyncTask([max_video_bw] (std::shared_ptr<MediaStream> stream) {
    if (stream->rtcp_processor_) {
      stream->rtcp_processor_->setMaxVideoBW(max_video_bw * 1000);
      if (stream->pipeline_) {
        stream->pipeline_->notifyUpdate();
      }
    }
  });
}

void MediaStream::syncClose() {
  ELOG_INFO("%s message:Close called", toLog());
  if (!sending_) {
    return;
  }
  sending_ = false;
  ready_ = false;
  video_sink_.reset();
  audio_sink_.reset();
  fb_sink_.reset();
  pipeline_initialized_ = false;
  pipeline_->close();
  pipeline_.reset();
  connection_.reset();
  ELOG_DEBUG("%s message: Close ended", toLog());
}

boost::future<void> MediaStream::close() {
  ELOG_DEBUG("%s message: Async close called", toLog());
  std::shared_ptr<MediaStream> shared_this = shared_from_this();
  return asyncTask([shared_this] (std::shared_ptr<MediaStream> stream) {
    shared_this->syncClose();
  });
}

void MediaStream::init() {
  if (source_fb_sink_.expired()) {
    source_fb_sink_ = std::dynamic_pointer_cast<FeedbackSink>(shared_from_this());
  }
  if (sink_fb_source_.expired()) {
    sink_fb_source_ = std::dynamic_pointer_cast<FeedbackSource>(shared_from_this());
  }
}

bool MediaStream::configure(bool doNotWaitForRemoteSdp) {
  if (doNotWaitForRemoteSdp) {
    ready_ = true;
  }
  return true;
}

bool MediaStream::isSourceSSRC(uint32_t ssrc) {
  return isVideoSourceSSRC(ssrc) || isAudioSourceSSRC(ssrc);
}

bool MediaStream::isSinkSSRC(uint32_t ssrc) {
  return isVideoSinkSSRC(ssrc) || isAudioSinkSSRC(ssrc);
}

bool MediaStream::setRemoteSdp(std::shared_ptr<SdpInfo> sdp, int session_version_negotiated = -1) {
  ELOG_DEBUG("%s message: setting remote SDP to Stream, sending: %d, initialized: %d",
    toLog(), sending_, pipeline_initialized_);
  if (!sending_) {
    return true;
  }

  std::shared_ptr<SdpInfo> remote_sdp = std::make_shared<SdpInfo>(*sdp.get());
  auto video_ssrc_list_it = remote_sdp->video_ssrc_map.find(getLabel());
  auto audio_ssrc_it = remote_sdp->audio_ssrc_map.find(getLabel());

  if (isPublisher() && !ready_) {
    bool stream_found = false;

    if (video_ssrc_list_it != remote_sdp->video_ssrc_map.end() ||
        audio_ssrc_it != remote_sdp->audio_ssrc_map.end()) {
      stream_found = true;
    }

    if (!stream_found) {
      return true;
    }
  }

  if (!isPublisher() && session_version_negotiated >= 0 && session_version_ > session_version_negotiated) {
    ELOG_WARN("%s message: too old session version, session_version_: %d, negotiated_session_version: %d",
        toLog(), session_version_, session_version_negotiated);
    return true;
  }

  remote_sdp_ = remote_sdp;

  if (remote_sdp_->videoBandwidth != 0) {
    ELOG_DEBUG("%s message: Setting remote BW, maxVideoBW: %u", toLog(), remote_sdp_->videoBandwidth);
    this->rtcp_processor_->setMaxVideoBW(remote_sdp_->videoBandwidth*1000);
  }

  ready_ = true;

  if (pipeline_initialized_ && pipeline_) {
    pipeline_->notifyUpdate();
    return true;
  }
  bundle_ = remote_sdp_->isBundle;
  if (video_ssrc_list_it != remote_sdp_->video_ssrc_map.end()) {
    setVideoSourceSSRCList(video_ssrc_list_it->second);
  }

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

  initializeStats();

  notifyMediaStreamEvent("ready", "");

  return true;
}

void MediaStream::initializeStats() {
  log_stats_->getNode().insertStat("streamId", StringStat{getId()});
  log_stats_->getNode().insertStat("audioBitrate", CumulativeStat{0});
  log_stats_->getNode().insertStat("audioFL", CumulativeStat{0});
  log_stats_->getNode().insertStat("audioPL", CumulativeStat{0});
  log_stats_->getNode().insertStat("audioJitter", CumulativeStat{0});
  log_stats_->getNode().insertStat("audioMuted", CumulativeStat{0});
  log_stats_->getNode().insertStat("audioNack", CumulativeStat{0});
  log_stats_->getNode().insertStat("audioRemb", CumulativeStat{0});
  log_stats_->getNode().insertStat("audioSrTimestamp", CumulativeStat{0});
  log_stats_->getNode().insertStat("audioSrNtp", CumulativeStat{0});

  log_stats_->getNode().insertStat("videoBitrate", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoFL", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoPL", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoJitter", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoMuted", CumulativeStat{0});
  log_stats_->getNode().insertStat("slideshow", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoNack", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoPli", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoFir", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoRemb", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoErizoRemb", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoKeyFrames", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoSrTimestamp", CumulativeStat{0});
  log_stats_->getNode().insertStat("videoSrNtp", CumulativeStat{0});

  log_stats_->getNode().insertStat("SL0TL0", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL0TL1", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL0TL2", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL0TL3", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL1TL0", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL1TL1", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL1TL2", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL1TL3", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL2TL0", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL2TL1", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL2TL2", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL2TL3", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL3TL0", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL3TL1", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL3TL2", CumulativeStat{0});
  log_stats_->getNode().insertStat("SL3TL3", CumulativeStat{0});

  log_stats_->getNode().insertStat("maxActiveSL", CumulativeStat{0});
  log_stats_->getNode().insertStat("maxActiveTL", CumulativeStat{0});
  log_stats_->getNode().insertStat("selectedSL", CumulativeStat{0});
  log_stats_->getNode().insertStat("selectedTL", CumulativeStat{0});
  log_stats_->getNode().insertStat("isPublisher", CumulativeStat{is_publisher_});

  log_stats_->getNode().insertStat("totalBitrate", CumulativeStat{0});
  log_stats_->getNode().insertStat("rtxBitrate", CumulativeStat{0});
  log_stats_->getNode().insertStat("paddingBitrate", CumulativeStat{0});
  log_stats_->getNode().insertStat("bwe", CumulativeStat{0});

  log_stats_->getNode().insertStat("maxVideoBW", CumulativeStat{0});
  log_stats_->getNode().insertStat("qualityCappedByConstraints", CumulativeStat{0});

  std::weak_ptr<MediaStream> weak_this = shared_from_this();
  worker_->scheduleEvery([weak_this] () {
    if (auto stream = weak_this.lock()) {
      if (stream->sending_) {
        stream->printStats();
        return true;
      }
    }
    return false;
  }, kStreamStatsPeriod);
}

void MediaStream::transferLayerStats(std::string spatial, std::string temporal) {
  std::string node = "SL" + spatial + "TL" + temporal;
  if (stats_->getNode().hasChild("qualityLayers") &&
      stats_->getNode()["qualityLayers"].hasChild(spatial) &&
      stats_->getNode()["qualityLayers"][spatial].hasChild(temporal)) {
    log_stats_->getNode()
      .insertStat(node, CumulativeStat{stats_->getNode()["qualityLayers"][spatial][temporal].value()});
  }
}

void MediaStream::transferMediaStats(std::string target_node, std::string source_parent, std::string source_node) {
  if (stats_->getNode().hasChild(source_parent) &&
      stats_->getNode()[source_parent].hasChild(source_node)) {
    log_stats_->getNode()
      .insertStat(target_node, CumulativeStat{stats_->getNode()[source_parent][source_node].value()});
  }
}

void MediaStream::printStats() {
  std::string video_ssrc;
  std::string audio_ssrc;

  log_stats_->getNode().insertStat("audioEnabled", CumulativeStat{audio_enabled_});
  log_stats_->getNode().insertStat("videoEnabled", CumulativeStat{video_enabled_});

  log_stats_->getNode().insertStat("maxVideoBW", CumulativeStat{getMaxVideoBW()});

  transferMediaStats("qualityCappedByConstraints", "qualityLayers", "qualityCappedByConstraints");

  if (audio_enabled_) {
    audio_ssrc = std::to_string(is_publisher_ ? getAudioSourceSSRC() : getAudioSinkSSRC());
    transferMediaStats("audioBitrate", audio_ssrc, "bitrateCalculated");
    transferMediaStats("audioPL",      audio_ssrc, "packetsLost");
    transferMediaStats("audioFL",      audio_ssrc, "fractionLost");
    transferMediaStats("audioJitter",  audio_ssrc, "jitter");
    transferMediaStats("audioMuted",   audio_ssrc, "erizoAudioMute");
    transferMediaStats("audioNack",    audio_ssrc, "NACK");
    transferMediaStats("audioRemb",    audio_ssrc, "bandwidth");
    transferMediaStats("audioSrTimestamp", audio_ssrc, "srTimestamp");
    transferMediaStats("audioSrNtp", audio_ssrc, "srNtp");
  }
  if (video_enabled_) {
    video_ssrc = std::to_string(is_publisher_ ? getVideoSourceSSRC() : getVideoSinkSSRC());
    transferMediaStats("videoBitrate", video_ssrc, "bitrateCalculated");
    transferMediaStats("videoPL",      video_ssrc, "packetsLost");
    transferMediaStats("videoFL",      video_ssrc, "fractionLost");
    transferMediaStats("videoJitter",  video_ssrc, "jitter");
    transferMediaStats("videoMuted",   audio_ssrc, "erizoVideoMute");
    transferMediaStats("slideshow",    video_ssrc, "erizoSlideShow");
    transferMediaStats("videoNack",    video_ssrc, "NACK");
    transferMediaStats("videoPli",     video_ssrc, "PLI");
    transferMediaStats("videoFir",     video_ssrc, "FIR");
    transferMediaStats("videoRemb",    video_ssrc, "bandwidth");
    transferMediaStats("videoErizoRemb", video_ssrc, "erizoBandwidth");
    transferMediaStats("videoKeyFrames", video_ssrc, "keyFrames");
    transferMediaStats("videoSrTimestamp", video_ssrc, "srTimestamp");
    transferMediaStats("videoSrNtp", video_ssrc, "srNtp");
  }

  for (uint32_t spatial = 0; spatial <= 3; spatial++) {
    for (uint32_t temporal = 0; temporal <= 3; temporal++) {
      transferLayerStats(std::to_string(spatial), std::to_string(temporal));
    }
  }

  transferMediaStats("maxActiveSL", "qualityLayers", "maxActiveSpatialLayer");
  transferMediaStats("maxActiveTL", "qualityLayers", "maxActiveTemporalLayer");
  transferMediaStats("selectedSL", "qualityLayers", "selectedSpatialLayer");
  transferMediaStats("selectedTL", "qualityLayers", "selectedTemporalLayer");
  transferMediaStats("totalBitrate", "total", "bitrateCalculated");
  transferMediaStats("paddingBitrate", "total", "paddingBitrate");
  transferMediaStats("rtxBitrate", "total", "rtxBitrate");
  transferMediaStats("bwe", "total", "senderBitrateEstimation");

  ELOG_INFOT(statsLogger, "%s", log_stats_->getStats());
}

void MediaStream::initializePipeline() {
  if (pipeline_initialized_) {
    return;
  }
  handler_manager_ = std::make_shared<HandlerManager>(shared_from_this());
  pipeline_->addService(shared_from_this());
  pipeline_->addService(handler_manager_);
  pipeline_->addService(rtcp_processor_);
  pipeline_->addService(stats_);
  pipeline_->addService(quality_manager_);
  pipeline_->addService(packet_buffer_);

  pipeline_->addFront(std::make_shared<PacketReader>(this));

  pipeline_->addFront(std::make_shared<RtcpProcessorHandler>());
  pipeline_->addFront(std::make_shared<FecReceiverHandler>());
  pipeline_->addFront(std::make_shared<LayerBitrateCalculationHandler>());
  pipeline_->addFront(std::make_shared<QualityFilterHandler>());
  pipeline_->addFront(std::make_shared<IncomingStatsHandler>());
  pipeline_->addFront(std::make_shared<FakeKeyframeGeneratorHandler>());
  pipeline_->addFront(std::make_shared<RtpTrackMuteHandler>());
  pipeline_->addFront(std::make_shared<RtpSlideShowHandler>());
  pipeline_->addFront(std::make_shared<RtpPaddingGeneratorHandler>());
  pipeline_->addFront(std::make_shared<PeriodicPliHandler>());
  pipeline_->addFront(std::make_shared<PliPriorityHandler>());
  pipeline_->addFront(std::make_shared<PliPacerHandler>());
  pipeline_->addFront(std::make_shared<RtpPaddingRemovalHandler>());
  pipeline_->addFront(std::make_shared<BandwidthEstimationHandler>());
  pipeline_->addFront(std::make_shared<RtcpFeedbackGenerationHandler>());
  pipeline_->addFront(std::make_shared<RtpRetransmissionHandler>());
  pipeline_->addFront(std::make_shared<SRPacketHandler>());
  pipeline_->addFront(std::make_shared<LayerDetectorHandler>());
  pipeline_->addFront(std::make_shared<OutgoingStatsHandler>());
  pipeline_->addFront(std::make_shared<PacketCodecParser>());

  pipeline_->addFront(std::make_shared<PacketWriter>(this));
  pipeline_->finalize();

  if (connection_) {
    quality_manager_->setConnectionQualityLevel(connection_->getConnectionQualityLevel());
  }
  pipeline_initialized_ = true;
}

int MediaStream::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  if (audio_enabled_) {
    sendPacketAsync(std::make_shared<DataPacket>(*audio_packet));
  }
  return audio_packet->length;
}

int MediaStream::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  if (video_enabled_) {
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

    if (stream_ptr->pipeline_) {
      stream_ptr->pipeline_->notifyEvent(event);
    }

    if (event->getType() == "PublisherRtpInfoEvent") {
      if (!stream_ptr->is_publisher_) {
        auto publisher_info_event = std::static_pointer_cast<PublisherRtpInfoEvent>(event);
        if (publisher_info_event->kind_ == "audio") {
          stream_ptr->publisher_info_.audio_fraction_lost = publisher_info_event->fraction_lost_;
        } else {
          stream_ptr->publisher_info_.video_fraction_lost = publisher_info_event->fraction_lost_;
        }
      }
    }
  });
  return 1;
}

void MediaStream::onTransportData(std::shared_ptr<DataPacket> incoming_packet, Transport *transport) {
  if (audio_sink_.expired() && video_sink_.expired() && fb_sink_.expired()) {
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

    if (stream_ptr->pipeline_) {
      stream_ptr->pipeline_->read(std::move(packet));
    }
  });
}

void MediaStream::read(std::shared_ptr<DataPacket> packet) {
  char* buf = packet->data;
  int len = packet->length;
  // PROCESS RTCP
  RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
  uint32_t recvSSRC = 0;
  auto video_sink = video_sink_.lock();
  auto audio_sink = audio_sink_.lock();
  if (!chead->isRtcp()) {
    recvSSRC = head->getSSRC();
  } else if (chead->packettype == RTCP_Sender_PT || chead->packettype == RTCP_SDES_PT) {  // Sender Report
    recvSSRC = chead->getSSRC();
  }
  // DELIVER FEEDBACK (RR, FEEDBACK PACKETS)
  if (chead->isFeedback()) {
    auto fb_sink = fb_sink_.lock();
    if (should_send_feedback_ && fb_sink) {
      fb_sink->deliverFeedback(std::move(packet));
    }
  } else {
    // RTP or RTCP Sender Report
    if (bundle_) {
      // Check incoming SSRC
      // Deliver data
      if (isVideoSourceSSRC(recvSSRC) && video_sink) {
        parseIncomingPayloadType(buf, len, VIDEO_PACKET);
        parseIncomingExtensionId(buf, len, VIDEO_PACKET);
        video_sink->deliverVideoData(std::move(packet));
      } else if (isAudioSourceSSRC(recvSSRC) && audio_sink) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        parseIncomingExtensionId(buf, len, AUDIO_PACKET);
        audio_sink->deliverAudioData(std::move(packet));
      } else {
        ELOG_DEBUG("%s read video unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                    toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
      }
    } else {
      if (packet->type == AUDIO_PACKET && audio_sink) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        parseIncomingExtensionId(buf, len, AUDIO_PACKET);
        // Firefox does not send SSRC in SDP
        if (getAudioSourceSSRC() == 0) {
          ELOG_DEBUG("%s discoveredAudioSourceSSRC:%u", toLog(), recvSSRC);
          setAudioSourceSSRC(recvSSRC);
        }
        audio_sink->deliverAudioData(std::move(packet));
      } else if (packet->type == VIDEO_PACKET && video_sink) {
        parseIncomingPayloadType(buf, len, VIDEO_PACKET);
        parseIncomingExtensionId(buf, len, VIDEO_PACKET);
        // Firefox does not send SSRC in SDP
        if (getVideoSourceSSRC() == 0) {
          ELOG_DEBUG("%s discoveredVideoSourceSSRC:%u", toLog(), recvSSRC);
          setVideoSourceSSRC(recvSSRC);
        }
        // change ssrc for RTP packets, don't touch here if RTCP
        video_sink->deliverVideoData(std::move(packet));
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
  if (auto event_sink = event_sink_.lock()) {
    event_sink->deliverEvent(std::move(event));
  }
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
  if (auto fb_sink = fb_sink_.lock()) {
    fb_sink->deliverFeedback(RtpUtils::createPLI(getVideoSinkSSRC(), getVideoSourceSSRC()));
  }
}

void MediaStream::setPeriodicKeyframeRequests(bool activate, uint32_t interval) {
  ELOG_DEBUG("%s message: settingPeriodicKeyframes, activate: %u, interval, %u", activate, interval);
  periodic_keyframes_requested_ = activate;
  periodic_keyframe_interval_ = interval;
  notifyUpdateToHandlers();
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
  changeDeliverExtensionId(packet.get(), packet->type);
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

void MediaStream::setTargetPaddingBitrate(uint64_t target_padding_bitrate) {
  target_padding_bitrate_ = target_padding_bitrate;
  notifyUpdateToHandlers();
}

uint32_t MediaStream::getTargetVideoBitrate() {
  bool slide_show_mode = isSlideShowModeEnabled();
  bool is_simulcast = isSimulcast();
  uint32_t bitrate_sent = getVideoBitrate();
  uint32_t max_bitrate = getMaxVideoBW();
  uint32_t bitrate_from_max_quality_layer = getBitrateFromMaxQualityLayer();

  uint32_t target_bitrate = max_bitrate;
  if (is_simulcast) {
    target_bitrate = std::min(bitrate_from_max_quality_layer, max_bitrate);
  }
  if (slide_show_mode || !is_simulcast) {
    target_bitrate = std::min(bitrate_sent, max_bitrate);
  }
  if (target_bitrate == 0) {
    target_bitrate = kInitialBitrate;
  }
  return target_bitrate;
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
    if (media_stream && media_stream->pipeline_) {
      media_stream->pipeline_->notifyUpdate();
    }
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
  for (const auto &item : metadata) {
    log_stats_->getNode().insertStat("metadata-" + item.first, StringStat{item.second});
  }
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

void MediaStream::changeDeliverExtensionId(DataPacket *dp, packetType type) {
  RtpHeader* h = reinterpret_cast<RtpHeader*>(dp->data);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(dp->data);
  if (!chead->isRtcp()) {
    // Extension Id to external
    if (h->getExtension()) {
      std::array<RTPExtensions, 15> extMap;
      RtpExtensionProcessor& ext_processor = getRtpExtensionProcessor();
      switch (type) {
        case VIDEO_PACKET:
          extMap = ext_processor.getVideoExtensionMap();
          break;
        case AUDIO_PACKET:
          extMap = ext_processor.getAudioExtensionMap();
          break;
        default:
          ELOG_WARN("%s Won't process RTP extensions for unknown type packets", toLog());
          return;
          break;
      }
      uint16_t totalExtLength = h->getExtLength();
      if (h->getExtId() == 0xBEDE) {  // One-Byte Header
        char* extBuffer = (char*)&h->extensions;  // NOLINT
        uint8_t extByte = 0;
        uint16_t currentPlace = 1;
        uint8_t extId = 0;
        uint8_t extLength = 0;
        while (currentPlace < (totalExtLength*4)) {
          extByte = (uint8_t)(*extBuffer);
          extId = extByte >> 4;
          extLength = extByte & 0x0F;
          // extId == 0 should never happen, see https://tools.ietf.org/html/rfc5285#section-4.2
          if (extId != 0) {
            for (int i = 1; i < 15; i++) {
              if (extMap.at(i) == extId) {
                extBuffer[0] = (extBuffer[0] | 0xF0) & (i << 4 | 0x0F);
              }
            }
          }
          extBuffer = extBuffer + extLength + 2;
          currentPlace = currentPlace + extLength + 2;
        }
      } else {
        ELOG_WARN("%s Two-Byte Header not handled!", toLog());
      }
    }
  }
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

void MediaStream::parseIncomingExtensionId(char *buf, int len, packetType type) {
  RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(buf);
  RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
  if (!chead->isRtcp()) {
    // Extension Id to internal
    if (h->getExtension()) {
      std::array<RTPExtensions, 15> extMap;
      RtpExtensionProcessor& ext_processor = getRtpExtensionProcessor();
      switch (type) {
        case VIDEO_PACKET:
          extMap = ext_processor.getVideoExtensionMap();
          break;
        case AUDIO_PACKET:
          extMap = ext_processor.getAudioExtensionMap();
          break;
        default:
          ELOG_WARN("%s Won't process RTP extensions for unknown type packets", toLog());
          return;
          break;
      }
      uint16_t totalExtLength = h->getExtLength();
      if (h->getExtId() == 0xBEDE) {  // One-Byte Header
        char* extBuffer = (char*)&h->extensions;  // NOLINT
        uint8_t extByte = 0;
        uint16_t currentPlace = 1;
        uint8_t extId = 0;
        uint8_t extLength = 0;
        while (currentPlace < (totalExtLength*4)) {
          extByte = (uint8_t)(*extBuffer);
          extId = extByte >> 4;
          extLength = extByte & 0x0F;
          if (extId != 0 && extMap[extId] != 0) {
            extBuffer[0] = (extBuffer[0] | 0xF0) & (extMap[extId] << 4 | 0x0F);
          }
          extBuffer = extBuffer + extLength + 2;
          currentPlace = currentPlace + extLength + 2;
        }
      } else {
        ELOG_WARN("%s Two-Byte Header not handled!", toLog());
      }
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
    connection_->send(packet);
  }
}

void MediaStream::enableHandler(const std::string &name) {
  asyncTask([name] (std::shared_ptr<MediaStream> conn) {
      if (conn && conn->pipeline_) {
        conn->pipeline_->enable(name);
      }
  });
}

void MediaStream::disableHandler(const std::string &name) {
  asyncTask([name] (std::shared_ptr<MediaStream> conn) {
    if (conn && conn->pipeline_) {
      conn->pipeline_->disable(name);
    }
  });
}

void MediaStream::notifyUpdateToHandlers() {
  asyncTask([] (std::shared_ptr<MediaStream> conn) {
    if (conn && conn->pipeline_) {
      conn->pipeline_->notifyUpdate();
    }
  });
}

boost::future<void> MediaStream::asyncTask(std::function<void(std::shared_ptr<MediaStream>)> f) {
  auto task_promise = std::make_shared<boost::promise<void>>();
  std::weak_ptr<MediaStream> weak_this = shared_from_this();
  worker_->task([weak_this, f, task_promise] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
    task_promise->set_value();
  });
  return task_promise->get_future();
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

  if (pipeline_) {
    pipeline_->write(std::move(p));
  }
}

void MediaStream::setQualityLayer(int spatial_layer, int temporal_layer) {
  asyncTask([spatial_layer, temporal_layer] (std::shared_ptr<MediaStream> media_stream) {
    media_stream->quality_manager_->forceLayers(spatial_layer, temporal_layer);
  });
}

void MediaStream::enableSlideShowBelowSpatialLayer(bool enabled, int spatial_layer) {
  asyncTask([enabled, spatial_layer] (std::shared_ptr<MediaStream> media_stream) {
    media_stream->quality_manager_->enableSlideShowBelowSpatialLayer(enabled, spatial_layer);
  });
}

}  // namespace erizo
