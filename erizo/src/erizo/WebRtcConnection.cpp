/*
 * WebRTCConnection.cpp
 */

#include <map>
#include <algorithm>
#include <string>
#include <cstring>
#include <vector>

#include "WebRtcConnection.h"
#include "MediaStream.h"
#include "Transceiver.h"
#include "DtlsTransport.h"
#include "UnencryptedTransport.h"
#include "SdpInfo.h"
#include "bandwidth/MaxVideoBWDistributor.h"
#include "bandwidth/StreamPriorityBWDistributor.h"
#include "bandwidth/TargetVideoBWDistributor.h"
#include "rtp/RtpHeaders.h"
#include "rtp/SenderBandwidthEstimationHandler.h"
#include "rtp/BandwidthEstimationHandler.h"
#include "rtp/RtpPaddingManagerHandler.h"
#include "rtp/RtpUtils.h"

namespace erizo {
DEFINE_LOGGER(WebRtcConnection, "WebRtcConnection");
log4cxx::LoggerPtr WebRtcConnection::ConnectionStatsLogger = log4cxx::Logger::getLogger("ConnectionStats");

static constexpr auto kConnectionStatsPeriod = std::chrono::seconds(120);

WebRtcConnection::WebRtcConnection(std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker,
    const std::string& connection_id, const IceConfig& ice_config, const std::vector<RtpMap> rtp_mappings,
    const std::vector<erizo::ExtMap> ext_mappings, bool enable_connection_quality_check,
    const BwDistributionConfig& distribution_config, bool encrypt_transport,
    WebRtcConnectionEventListener* listener) :
    connection_id_{connection_id}, bundle_{false}, conn_event_listener_{listener},
    ice_config_{ice_config}, rtp_mappings_{rtp_mappings}, extension_processor_{ext_mappings},
    worker_{worker}, io_worker_{io_worker},
    remote_sdp_{std::make_shared<SdpInfo>(rtp_mappings)}, local_sdp_{std::make_shared<SdpInfo>(rtp_mappings)},
    audio_muted_{false}, video_muted_{false}, first_remote_sdp_processed_{false},
    enable_connection_quality_check_{enable_connection_quality_check}, encrypt_transport_{encrypt_transport},
    connection_target_bw_{0},
    pipeline_{Pipeline::create()},
    pipeline_initialized_{false}, latest_mid_{0} {
  stats_ = std::make_shared<Stats>();
  log_stats_ = std::make_shared<Stats>();
  this->setBwDistributionConfigSync(distribution_config);
  ELOG_INFO("%s message: constructor, stunserver: %s, stunPort: %d, minPort: %d, maxPort: %d, distributor: %u",
      toLog(), ice_config.stun_server.c_str(), ice_config.stun_port, ice_config.min_port, ice_config.max_port,
      distribution_config.selected_distributor);
  global_state_ = CONN_INITIAL;

  trickle_enabled_ = ice_config_.should_trickle;
  slide_show_mode_ = false;

  local_sdp_->isIceLite = ice_config_.ice_lite;

  sending_ = true;
}

WebRtcConnection::~WebRtcConnection() {
  ELOG_DEBUG("%s message:Destructor called", toLog());
  ELOG_DEBUG("%s message: Destructor ended", toLog());
}

boost::future<void> WebRtcConnection::syncClose() {
  ELOG_DEBUG("%s message: Close called", toLog());
  auto shared_this = shared_from_this();
  auto close_promise = std::make_shared<boost::promise<void>>();
  if (!sending_) {
    ELOG_DEBUG("%s message: Already closed, returning", toLog());
    close_promise->set_value();
    return close_promise->get_future();
  }
  sending_ = false;
  transceivers_.clear();
  streams_.clear();
  auto close_transport_futures = std::make_shared<std::vector<boost::future<void>>>();
  if (video_transport_.get()) {
    close_transport_futures->push_back(video_transport_->close());
  }
  if (audio_transport_.get()) {
    close_transport_futures->push_back(audio_transport_->close());
  }
  auto future_when_all = boost::when_all(close_transport_futures->begin(), close_transport_futures->end());
  future_when_all.then([shared_this, close_promise](decltype(future_when_all)) {
    shared_this->global_state_ = CONN_FINISHED;
    if (shared_this->conn_event_listener_ != nullptr) {
      shared_this->conn_event_listener_ = nullptr;
    }
    shared_this->pipeline_initialized_ = false;
    shared_this->pipeline_->close();
    shared_this->pipeline_.reset();
    ELOG_DEBUG("%s message: Close ended", shared_this->toLog());
    close_promise->set_value();
  });
  return close_promise->get_future();
}

boost::future<void> WebRtcConnection::close() {
  ELOG_DEBUG("%s message: Async close called", toLog());
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  auto task_promise = std::make_shared<boost::promise<void>>();
  worker_->task([weak_this, task_promise] {
    if (auto this_ptr = weak_this.lock()) {
      this_ptr->syncClose().then([task_promise] (boost::future<void>) {
        task_promise->set_value();
      });
    } else {
      task_promise->set_value();
    }
  });
  return task_promise->get_future();
}

bool WebRtcConnection::init() {
  asyncTask([] (std::shared_ptr<WebRtcConnection> connection) {
    connection->initializePipeline();
    connection->initializeStats();
    connection->maybeNotifyWebRtcConnectionEvent(connection->global_state_, "");
  });
  return true;
}

void WebRtcConnection::initializePipeline() {
  if (pipeline_initialized_) {
    return;
  }
  handler_manager_ = std::make_shared<HandlerManager>(shared_from_this());
  pipeline_->addService(shared_from_this());
  pipeline_->addService(handler_manager_);
  pipeline_->addService(stats_);

  pipeline_->addFront(std::make_shared<ConnectionPacketReader>(this));

  pipeline_->addFront(std::make_shared<BandwidthEstimationHandler>());

  pipeline_->addFront(std::make_shared<SenderBandwidthEstimationHandler>());
  pipeline_->addFront(std::make_shared<RtpPaddingManagerHandler>());

  pipeline_->addFront(std::make_shared<ConnectionPacketWriter>(this));
  pipeline_->finalize();
  pipeline_initialized_ = true;
}

void WebRtcConnection::initializeStats() {
  log_stats_->getNode().insertStat("connectionId", StringStat{connection_id_});
  log_stats_->getNode().insertStat("bwe", CumulativeStat{0});
  log_stats_->getNode().insertStat("bwDistributionAlgorithm", StringStat{""});
  log_stats_->getNode().insertStat("bwPriorityStrategy", StringStat{""});
  log_stats_->getNode().insertStat("paddingMode", CumulativeStat{0});

  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  worker_->scheduleEvery([weak_this] () {
    if (auto connection = weak_this.lock()) {
      if (connection->sending_) {
        connection->printStats();
        return true;
      }
    }
    return false;
  }, kConnectionStatsPeriod);
}

void WebRtcConnection::transferMediaStats(std::string target_node, std::string source_parent, std::string source_node) {
  if (stats_->getNode().hasChild(source_parent) &&
      stats_->getNode()[source_parent].hasChild(source_node)) {
    log_stats_->getNode()
      .insertStat(target_node, CumulativeStat{stats_->getNode()[source_parent][source_node].value()});
  }
}

void WebRtcConnection::printStats() {
  log_stats_->getNode().insertStat("bwDistributionAlgorithm",
      CumulativeStat(bw_distribution_config_.selected_distributor));
  log_stats_->getNode().insertStat("bwPriorityStrategy",
      StringStat{bw_distribution_config_.priority_strategy.getStrategyId()});
  log_stats_->getNode().insertStat("connectionQualityLevel",
      CumulativeStat(getConnectionQualityLevel()));
  transferMediaStats("bwe", "total", "senderBitrateEstimation");
  transferMediaStats("paddingMode", "total", "paddingMode");

  ELOG_INFOT(ConnectionStatsLogger, "%s", log_stats_->getStats());
}

void WebRtcConnection::notifyUpdateToHandlers() {
  asyncTask([] (std::shared_ptr<WebRtcConnection> conn) {
    if (conn && conn->pipeline_ && conn->pipeline_initialized_) {
      conn->pipeline_->notifyUpdate();
    }
  });
}

boost::future<void> WebRtcConnection::createOffer(bool bundle) {
  return asyncTask([bundle] (std::shared_ptr<WebRtcConnection> connection) {
    connection->createOfferSync(bundle);
  });
}

std::shared_ptr<MediaStream> WebRtcConnection::getMediaStream(std::string stream_id) {
  std::shared_ptr<MediaStream> stream;
  auto stream_it = std::find_if(streams_.begin(), streams_.end(),
      [stream_id] (const std::shared_ptr<MediaStream> &stream_in_vector) {
          return (stream_id == stream_in_vector->getId());
  });
  if (stream_it != streams_.end()) {
    stream = *stream_it;
  }
  return stream;
}

bool WebRtcConnection::createOfferSync(bool bundle) {
  boost::mutex::scoped_lock lock(update_state_mutex_);
  bundle_ = bundle;
  local_sdp_->createOfferSdp(true, true, bundle_);

  local_sdp_->dtlsRole = ACTPASS;
  if (local_sdp_->internal_dtls_role == ACTPASS) {
    local_sdp_->internal_dtls_role = PASSIVE;
  } else {
    local_sdp_->dtlsRole = local_sdp_->internal_dtls_role;
  }

  ELOG_DEBUG("%s message: Creating sdp offer, isBundle: %d, setup: %d",
    toLog(), bundle_, local_sdp_->internal_dtls_role);

  forEachMediaStream([this] (const std::shared_ptr<MediaStream> &media_stream) {
    if (media_stream->isPublisher()) {
      ELOG_DEBUG("%s message: getting local SDPInfo stream not running, stream_id: %s", toLog(), media_stream->getId());
      return;
    }

    // We associate new subscribed streams that have not been associated yet
    if (!media_stream->isReady()) {
      associateMediaStreamToSender(media_stream);
      media_stream->configure(true);
    }

    std::vector<uint32_t> video_ssrc_list = std::vector<uint32_t>();
    if (media_stream->getVideoSinkSSRC() != kDefaultVideoSinkSSRC && media_stream->getVideoSinkSSRC() != 0) {
      video_ssrc_list.push_back(media_stream->getVideoSinkSSRC());
    }
    ELOG_DEBUG("%s message: getting local SDPInfo, stream_id: %s, audio_ssrc: %u",
                toLog(), media_stream->getId(), media_stream->getAudioSinkSSRC());
    if (!video_ssrc_list.empty()) {
      local_sdp_->video_ssrc_map[media_stream->getLabel()] = video_ssrc_list;
    }
    if (media_stream->getAudioSinkSSRC() != kDefaultAudioSinkSSRC && media_stream->getAudioSinkSSRC() != 0) {
      local_sdp_->audio_ssrc_map[media_stream->getLabel()] = media_stream->getAudioSinkSSRC();
    }
  });

  // We reset receivers that have been removed
  forEachTransceiver([this] (const std::shared_ptr<Transceiver> &transceiver) {
    if (!transceiver->hasReceiver()) {
      return;
    }
    std::string stream_id = transceiver->getReceiver()->getId();
    auto stream = getMediaStream(stream_id);
    if (!stream) {
      transceiver->resetReceiver();
    }
  });

  auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());

  if (encrypt_transport_) {
    local_sdp_->profile = SAVPF;
  } else {
    local_sdp_->profile = AVPF;
  }

  if (bundle_) {
    if (video_transport_.get() == nullptr) {
      if (encrypt_transport_) {
        video_transport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              listener, ice_config_ , "", "", true, worker_, io_worker_));
      } else {
        video_transport_.reset(new UnencryptedTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              listener, ice_config_ , "", "", true, worker_, io_worker_));
      }
      video_transport_->copyLogContextFrom(*this);
      video_transport_->start();
    }
  } else {
    if (video_transport_.get() == nullptr) {
      if (encrypt_transport_) {
        // For now we don't re/check transports, if they are already created we leave them there
        video_transport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                                listener, ice_config_ , "", "", true, worker_, io_worker_));

      } else {
        video_transport_.reset(new UnencryptedTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              listener, ice_config_ , "", "", true, worker_, io_worker_));
      }
      video_transport_->copyLogContextFrom(*this);
      video_transport_->start();
    }
    if (audio_transport_.get() == nullptr) {
      if (encrypt_transport_) {
        audio_transport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, true,
                                                listener, ice_config_, "", "", true, worker_, io_worker_));
      } else {
        audio_transport_.reset(new UnencryptedTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, true,
                                                listener, ice_config_ , "", "", true, worker_, io_worker_));
      }
      audio_transport_->copyLogContextFrom(*this);
      audio_transport_->start();
    }
  }
  std::string msg = "";
  maybeNotifyWebRtcConnectionEvent(global_state_, msg);

  return true;
}

ConnectionQualityLevel WebRtcConnection::getConnectionQualityLevel() {
  if (distributor_->tooLowBandwidthEstimation()) {
    return ConnectionQualityLevel::VERY_LOW;
  }
  return ConnectionQualityLevel::GOOD;
}

void WebRtcConnection::associateMediaStreamToTransceiver(std::shared_ptr<MediaStream> media_stream,
    std::shared_ptr<Transceiver> transceiver) {
  bool is_audio = transceiver->getKind() == "audio";
  bool is_video = transceiver->getKind() == "video";
  if (is_audio && !media_stream->hasAudio()) {
    ELOG_WARN("%s message: Tried to link stream without audio to audio transceiver, streamId: %s, transceiverId: %d",
      toLog(), media_stream->getId(), transceiver->getId());
    return;
  } else if (is_video && !media_stream->hasVideo()) {
    ELOG_WARN("%s message: Tried to link stream without video a video transceiver, streamId: %s, transceiverId: %d",
    toLog(), media_stream->getId(), transceiver->getId());
    return;
  }
  if (media_stream->isPublisher()) {
    ELOG_DEBUG("Associating Receiver to Transceiver, %s", media_stream->getLabel());
    transceiver->setReceiver(media_stream);
  } else {
    ELOG_DEBUG("Associating Sender to Transceiver, %d, %d, %s",
      media_stream->getVideoSinkSSRC(), media_stream->getAudioSinkSSRC(), media_stream->getLabel());
    transceiver->setSender(media_stream);
  }
  if (is_audio) {
    media_stream->setAudioMid(transceiver->getId());
  } else {
    media_stream->setVideoMid(transceiver->getId());
  }
}

void WebRtcConnection::associateMediaStreamToSender(std::shared_ptr<MediaStream> media_stream) {
  if (media_stream->isPublisher()) {
    return;
  }
  ELOG_DEBUG("%s message: Process transceivers for local sdp, streams: %d, transceivers: %d",
    toLog(), streams_.size(), transceivers_.size());
  bool found = false;
  bool audio_found = false;
  bool video_found = false;
  if (!media_stream->hasAudio()) {
    audio_found = true;
  }
  if (!media_stream->hasVideo()) {
    video_found = true;
  }
  std::shared_ptr<Transceiver> audio_transceiver;
  std::shared_ptr<Transceiver> video_transceiver;
  std::for_each(transceivers_.begin(), transceivers_.end(),
      [&media_stream, &found, &audio_found,
      &video_found, &audio_transceiver, &video_transceiver, this] (const auto &transceiver) {
    if (found) {
      return;
    }
    if (!transceiver->isStopped()) {
      return;
    }
    if (!audio_found) {
      audio_transceiver = transceiver;
      audio_found = true;
    } else if (!video_found) {
      video_transceiver = transceiver;
      video_found = true;
    }
    if (audio_found && video_found) {
      found = true;
    }
    ELOG_DEBUG("%s message: mediaStream reusing transceiver, id: %s, transceiverId: %d",
      toLog(), media_stream->getId().c_str(), transceiver->getId());
  });
  ELOG_DEBUG("%s message: Transceivers for the new mediastream, id: %s, audio: %d, video %d",
        toLog(), media_stream->getId().c_str(), media_stream->hasAudio(), media_stream->hasVideo());
  if (media_stream->hasAudio()) {
    if (!audio_found) {
      audio_transceiver = std::make_shared<Transceiver>(latest_mid_++, "audio");
      ELOG_DEBUG("%s message: created transceiver, id: %s, transceiverId: %s",
        toLog(), media_stream->getId(), audio_transceiver->getId());
      transceivers_.push_back(audio_transceiver);
    } else {
      auto transceiver = std::make_shared<Transceiver>(latest_mid_++, "audio");
      std::replace(transceivers_.begin(), transceivers_.end(), audio_transceiver, transceiver);
      audio_transceiver = transceiver;
    }
    associateMediaStreamToTransceiver(media_stream, audio_transceiver);
  }
  if (media_stream->hasVideo()) {
    if (!video_found) {
      video_transceiver = std::make_shared<Transceiver>(latest_mid_++, "video");
      ELOG_DEBUG("%s message: created transceiver, id: %s, transceiverId: %s",
        toLog(), media_stream->getId(), video_transceiver->getId());
      transceivers_.push_back(video_transceiver);
    } else {
      auto transceiver = std::make_shared<Transceiver>(latest_mid_++, "video");
      std::replace(transceivers_.begin(), transceivers_.end(), video_transceiver, transceiver);
      video_transceiver = transceiver;
    }
    associateMediaStreamToTransceiver(media_stream, video_transceiver);
  }
  ELOG_DEBUG("%s message: Process transceivers end for local sdp, streams: %d, transceivers: %d",
    toLog(), streams_.size(), transceivers_.size());
}

boost::future<void> WebRtcConnection::addMediaStream(std::shared_ptr<MediaStream> media_stream) {
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  auto task_promise = std::make_shared<boost::promise<void>>();
  worker_->task([weak_this, task_promise, media_stream] {
    if (auto this_ptr = weak_this.lock()) {
      this_ptr->addMediaStreamSync(media_stream);
    }
    task_promise->set_value();
  });
  return task_promise->get_future();
}

void WebRtcConnection::addMediaStreamSync(std::shared_ptr<MediaStream> media_stream) {
  boost::mutex::scoped_lock lock(update_state_mutex_);
  ELOG_DEBUG("%s message: Adding mediaStream, id: %s, streams_count: %d",
    toLog(), media_stream->getId().c_str(), streams_.size());
  streams_.push_back(media_stream);
}

boost::future<void> WebRtcConnection::removeMediaStream(const std::string& stream_id) {
  return asyncTask([stream_id] (std::shared_ptr<WebRtcConnection> connection) {
    boost::mutex::scoped_lock lock(connection->update_state_mutex_);
    ELOG_DEBUG("%s message: removing mediaStream, id: %s", connection->toLog(), stream_id.c_str());
    auto stream_it = std::find_if(connection->streams_.begin(), connection->streams_.end(),
      [stream_id] (const std::shared_ptr<MediaStream> &stream_in_vector) {
          return (stream_id == stream_in_vector->getId());
    });
    if (stream_it == connection->streams_.end()) {
      return;
    }
    std::shared_ptr<MediaStream> stream = *stream_it;
    ELOG_DEBUG("%s message: removing mediaStream found, id: %s, publisher: %d",
      connection->toLog(), stream_id.c_str(), stream->isPublisher());
    auto video_it = connection->local_sdp_->video_ssrc_map.find(stream->getLabel());
    if (video_it != connection->local_sdp_->video_ssrc_map.end()) {
      connection->local_sdp_->video_ssrc_map.erase(video_it);
    }
    auto audio_it = connection->local_sdp_->audio_ssrc_map.find(stream->getLabel());
    if (audio_it != connection->local_sdp_->audio_ssrc_map.end()) {
      connection->local_sdp_->audio_ssrc_map.erase(audio_it);
    }

    connection->streams_.erase(stream_it);
    if (!stream->isPublisher()) {
      connection->forEachTransceiver([&stream](const std::shared_ptr<Transceiver> &transceiver) {
        if (transceiver->getSender() == stream) {
          transceiver->resetSender();
        }
      });
    }
  });
}

void WebRtcConnection::forEachMediaStream(std::function<void(const std::shared_ptr<MediaStream>&)> func) {
  std::for_each(streams_.begin(), streams_.end(), [func] (const auto &stream) {
    func(stream);
  });
}

void WebRtcConnection::forEachTransceiver(std::function<void(const std::shared_ptr<Transceiver>&)> func) {
  std::for_each(transceivers_.begin(), transceivers_.end(), [func] (const auto &transceiver) {
    func(transceiver);
  });
}

boost::future<void> WebRtcConnection::forEachMediaStreamAsync(
    std::function<void(const std::shared_ptr<MediaStream>&)> func) {
  auto futures = std::make_shared<std::vector<boost::future<void>>>();
  std::for_each(streams_.begin(), streams_.end(),
    [func, futures] (const auto &stream) {
      futures->push_back(stream->asyncTask(
          [func] (const std::shared_ptr<MediaStream> &stream) {
        func(stream);
      }));
  });

  auto future_when = boost::when_all(futures->begin(), futures->end());
  return future_when.then([](decltype(future_when)) {
    });
}

void WebRtcConnection::forEachMediaStreamAsyncNoPromise(
    std::function<void(const std::shared_ptr<MediaStream>&)> func) {
  std::for_each(streams_.begin(), streams_.end(), [func] (auto stream) {
    stream->asyncTask(
        [func] (const std::shared_ptr<MediaStream> &stream) {
      func(stream);
    });
  });
}

boost::future<void> WebRtcConnection::setRemoteSdpInfo(
    std::shared_ptr<SdpInfo> sdp) {
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  auto task_promise = std::make_shared<boost::promise<void>>();
  worker_->task([weak_this, sdp, task_promise] {
    if (auto connection = weak_this.lock()) {
      ELOG_DEBUG("%s message: setting remote SDPInfo", connection->toLog());
      if (!connection->sending_) {
        task_promise->set_value();
        return;
      }
      connection->remote_sdp_ = sdp;
      connection->notifyUpdateToHandlers();
      boost::future<void> future = connection->processRemoteSdp().then(
        [task_promise] (boost::future<void>) {
          task_promise->set_value();
        });
      return;
    }
    task_promise->set_value();
  });

  return task_promise->get_future();
}

void WebRtcConnection::copyDataToLocalSdpInfo(std::shared_ptr<SdpInfo> sdp_info) {
  asyncTask([sdp_info] (std::shared_ptr<WebRtcConnection> connection) {
    if (connection->sending_) {
      connection->local_sdp_->copyInfoFromSdp(sdp_info);
      connection->local_sdp_->updateSupportedExtensionMap(connection->extension_processor_.getSupportedExtensionMap());
    }
  });
}

boost::future<std::shared_ptr<SdpInfo>> WebRtcConnection::getLocalSdpInfo() {
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  auto task_promise = std::make_shared<boost::promise<std::shared_ptr<SdpInfo>>>();
  worker_->task([weak_this, task_promise] {
    std::shared_ptr<SdpInfo> info;
    if (auto this_ptr = weak_this.lock()) {
      info = this_ptr->getLocalSdpInfoSync();
    } else {
      ELOG_WARN("message: Error trying to getLocalSdpInfo - cannot lock WebrtcConnection, returning empty");
    }
    task_promise->set_value(info);
  });
  return task_promise->get_future();
}

void WebRtcConnection::associateTransceiversToSdpSync() {
  ELOG_DEBUG("%s message: getting local SDPInfo link receivers %d", toLog(), transceivers_.size());
  int index = 0;
  local_sdp_->medias.clear();
  forEachTransceiver([this, &index] (const std::shared_ptr<Transceiver> &transceiver) {
    StreamDirection direction = INACTIVE;
    std::string dir = "inactive";
    std::string sender_stream_id("");
    std::string receiver_stream_id("");
    std::string transceiver_id(transceiver->getId());
    bool just_added_to_sdp = !transceiver->hasBeenAddedToSdp();
    bool stopped = transceiver->isStopped();
    transceiver->setAsAddedToSdp();
    if (transceiver->hasSender()) {
      sender_stream_id = transceiver->getSenderLabel();
      direction = SENDONLY;
      dir = "sendonly";
    }
    if (transceiver->hasReceiver()) {
      receiver_stream_id = transceiver->getReceiverLabel();
      std::shared_ptr<MediaStream> stream = getMediaStreamFromLabel(receiver_stream_id);
      // We don't add it if the stream was removed
      if (stream) {
        if (direction == SENDONLY) {
          direction = SENDRECV;
          dir = "sendrecv";
        } else {
          direction = RECVONLY;
          dir = "recvonly";
        }
      } else {
        transceiver->resetReceiver();
      }
    }

    // We don't stop the transceiver completely until we make sure it is sent as Inactive in an SDP, to avoid
    // being reused before setting it to inactive.
    // We don't stop the first transceiver because it is linked to the tagged m= section
    // and if we stop it we will need to trigger an ICE restart.
    if (transceiver->isInactive() && index != 0) {
      transceiver->stop();
      stopped = true;
    }

    ELOG_DEBUG("%s message: transceiver, sender: %s, receiver: %s, transceiver: %s, direction: %s, justAdded: %d",
      toLog(), sender_stream_id, receiver_stream_id, transceiver_id, dir, just_added_to_sdp);
    SdpMediaInfo info(transceiver_id, sender_stream_id, receiver_stream_id, direction,
      transceiver->getKind(), transceiver->getSsrc(), just_added_to_sdp, stopped);
    local_sdp_->medias.push_back(info);
    index++;
  });
}

std::shared_ptr<SdpInfo> WebRtcConnection::getLocalSdpInfoSync() {
  boost::mutex::scoped_lock lock(update_state_mutex_);
  ELOG_DEBUG("%s message: getting local SDPInfo", toLog());
  forEachMediaStream([this] (const std::shared_ptr<MediaStream> &media_stream) {
    if (!media_stream->isReady() || media_stream->isPublisher()) {
      ELOG_DEBUG("%s message: getting local SDPInfo stream not running, stream_id: %s",
        toLog(), media_stream->getId());
      return;
    }
    std::vector<uint32_t> video_ssrc_list = std::vector<uint32_t>();
    if (media_stream->getVideoSinkSSRC() != kDefaultVideoSinkSSRC && media_stream->getVideoSinkSSRC() != 0) {
      video_ssrc_list.push_back(media_stream->getVideoSinkSSRC());
    }
    ELOG_DEBUG("%s message: getting local SDPInfo, stream_id: %s, audio_ssrc: %u",
               toLog(), media_stream->getId(), media_stream->getAudioSinkSSRC());
    if (!video_ssrc_list.empty()) {
      local_sdp_->video_ssrc_map[media_stream->getLabel()] = video_ssrc_list;
    }
    if (media_stream->getAudioSinkSSRC() != kDefaultAudioSinkSSRC && media_stream->getAudioSinkSSRC() != 0) {
      local_sdp_->audio_ssrc_map[media_stream->getLabel()] = media_stream->getAudioSinkSSRC();
    }
  });
  associateTransceiversToSdpSync();

  bool sending_audio = local_sdp_->audio_ssrc_map.size() > 0;
  bool sending_video = local_sdp_->video_ssrc_map.size() > 0;

  int audio_sources = 0;
  for (auto iterator = remote_sdp_->audio_ssrc_map.begin(), itr_end = remote_sdp_->audio_ssrc_map.end();
      iterator != itr_end; ++iterator) {
    audio_sources++;
  }
  int video_sources = 0;
  for (auto iterator = remote_sdp_->video_ssrc_map.begin(), itr_end = remote_sdp_->video_ssrc_map.end();
      iterator != itr_end; ++iterator) {
    video_sources += iterator->second.size();
  }

  bool receiving_audio = audio_sources > 0;
  bool receiving_video = video_sources > 0;

  if (!sending_audio && receiving_audio) {
    local_sdp_->audioDirection = erizo::RECVONLY;
  } else if (sending_audio && !receiving_audio) {
    local_sdp_->audioDirection = erizo::SENDONLY;
  } else if (sending_audio && receiving_audio) {
    local_sdp_->audioDirection = erizo::SENDRECV;
  } else {
    local_sdp_->audioDirection = erizo::INACTIVE;
  }

  if (!sending_video && receiving_video) {
    local_sdp_->videoDirection = erizo::RECVONLY;
  } else if (sending_video && !receiving_video) {
    local_sdp_->videoDirection = erizo::SENDONLY;
  } else if (sending_video && receiving_video) {
    local_sdp_->videoDirection = erizo::SENDRECV;
  } else {
    local_sdp_->videoDirection = erizo::INACTIVE;
  }

  if (video_transport_) {
    video_transport_->processLocalSdp(local_sdp_.get());
  }
  if (!bundle_ && audio_transport_) {
    audio_transport_->processLocalSdp(local_sdp_.get());
  }
  local_sdp_->profile = remote_sdp_->profile;

  auto local_sdp_copy = std::make_shared<SdpInfo>(*local_sdp_.get());
  return local_sdp_copy;
}

boost::future<void> WebRtcConnection::setRemoteSdpsToMediaStreams() {
  ELOG_DEBUG("%s message: setting remote SDP, transceivers: %d", toLog(), transceivers_.size());
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  std::shared_ptr<SdpInfo> remote_sdp = std::make_shared<SdpInfo>(*remote_sdp_.get());
  return forEachMediaStreamAsync([weak_this, remote_sdp]
      (std::shared_ptr<MediaStream> media_stream) {
    if (auto connection = weak_this.lock()) {
      media_stream->setRemoteSdp(remote_sdp);
      ELOG_DEBUG("%s message: setting remote SDP to stream, stream: %s",
        connection->toLog(), media_stream->getId());
    }
  });
}

std::shared_ptr<MediaStream> WebRtcConnection::getMediaStreamFromLabel(std::string stream_label) {
  std::shared_ptr<MediaStream> media_stream;
  auto stream_it = std::find_if(streams_.begin(), streams_.end(),
      [stream_label] (const std::shared_ptr<MediaStream> &stream_in_vector) {
          return (stream_label == stream_in_vector->getLabel());
    });
  if (stream_it != streams_.end()) {
    media_stream = *stream_it;
  }
  return media_stream;
}

void WebRtcConnection::detectNewTransceiversInRemoteSdp() {
  // We don't check directions of previous transceivers because we manage
  // that by adding and removing media streams.
  size_t index = 0;
  ELOG_DEBUG("%s message: Process transceivers from remote sdp, size: %d, localSize: %d",
    toLog(), remote_sdp_->medias.size(), transceivers_.size());
  for (const SdpMediaInfo &media_info : remote_sdp_->medias) {
    uint32_t mid = std::stoi(media_info.mid);
    if (index >= transceivers_.size()) {
      ELOG_DEBUG("%s message: New Transceiver received, mid: %s, index: %d, kind: %s, label: %s",
        toLog(), media_info.mid, index, media_info.kind, media_info.sender_id);
      auto transceiver = std::make_shared<Transceiver>(media_info.mid, media_info.kind);
      transceiver->setAsAddedToSdp();
      if (mid > latest_mid_) {
        latest_mid_ = mid + 1;
      }
      transceivers_.push_back(transceiver);
      if (!media_info.sender_id.empty()) {
        auto media_stream = getMediaStreamFromLabel(media_info.sender_id);
        if (media_stream) {
          ELOG_DEBUG("%s message: Associating MediaStream to transceiver, label: %s, mid: %s",
            toLog(), media_stream->getLabel(), transceiver->getId());
          associateMediaStreamToTransceiver(media_stream, transceiver);
        } else {
          ELOG_DEBUG("%s message: SDP MediaInfo belongs to no receiver stream, streamId: %s, transceiverId: %d",
            toLog(), media_info.sender_id, index);
        }
      }
      if (media_info.stopped) {
        transceiver->stop();
      }
    } else {
      auto transceiver = transceivers_[index];

      if (transceiver->getId() != media_info.mid) {
        auto old_transceiver = transceiver;
        transceiver = std::make_shared<Transceiver>(media_info.mid, media_info.kind);
        transceiver->setAsAddedToSdp();
        old_transceiver->resetReceiver();
        old_transceiver->resetSender();
        ELOG_DEBUG("%s message: Replacing transceiver, oldMid: %s, mid: %s, index: %d, kind: %s, label: %s",
        toLog(), old_transceiver->getId(), media_info.mid, index, media_info.kind, media_info.sender_id);
        // We don't currently stop the media streams (sender and receiver) because
        // we rely on the negotiation to do that part.
        std::replace(transceivers_.begin(), transceivers_.end(), old_transceiver, transceiver);
      }
      if (mid > latest_mid_) {
        latest_mid_ = mid + 1;
      }
      if (!media_info.sender_id.empty()) {
        auto media_stream = getMediaStreamFromLabel(media_info.sender_id);
        if (media_stream) {
          ELOG_DEBUG("%s message: Associating MediaStream to reused transceiver, label: %s, mid: %s",
            toLog(), media_stream->getLabel(), transceiver->getId());
          associateMediaStreamToTransceiver(media_stream, transceiver);
        } else {
          ELOG_DEBUG("%s message: We received a transceiver with an unknown remote stream, streamId: %s, mid: %d",
            toLog(), media_info.sender_id, index);
        }
      }
      if (media_info.stopped) {
        transceiver->stop();
      }
    }
    index++;
  }
  ELOG_DEBUG("%s message: Process transceivers end from remote sdp, size: %d, localSize: %d",
    toLog(), remote_sdp_->medias.size(), transceivers_.size());
}

boost::future<void> WebRtcConnection::processRemoteSdp() {
  ELOG_DEBUG("%s message: processing remote SDP", toLog());
  if (!first_remote_sdp_processed_ && local_sdp_->internal_dtls_role == ACTPASS) {
    local_sdp_->internal_dtls_role = ACTIVE;
  }
  local_sdp_->dtlsRole = local_sdp_->internal_dtls_role;
  ELOG_DEBUG("%s message: process remote sdp, setup: %d", toLog(), local_sdp_->internal_dtls_role);

  detectNewTransceiversInRemoteSdp();

  local_sdp_->setOfferSdp(remote_sdp_);
  extension_processor_.setSdpInfo(local_sdp_);
  notifyUpdateToHandlers();
  local_sdp_->updateSupportedExtensionMap(extension_processor_.getSupportedExtensionMap());

  if (first_remote_sdp_processed_) {
    return setRemoteSdpsToMediaStreams();
  }

  bundle_ = remote_sdp_->isBundle;

  if (remote_sdp_->profile == SAVPF) {
    ELOG_DEBUG("%s message: creating encrypted transports", toLog());
    if (remote_sdp_->isFingerprint) {
      auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());
      if (bundle_) {
        std::string username = remote_sdp_->getUsername(VIDEO_TYPE);
        std::string password = remote_sdp_->getPassword(VIDEO_TYPE);
        if (video_transport_.get() == nullptr) {
          ELOG_DEBUG("%s message: Creating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          video_transport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, remote_sdp_->isRtcpMux,
                                                  listener, ice_config_ , username, password, false,
                                                  worker_, io_worker_));
          video_transport_->copyLogContextFrom(*this);
          video_transport_->start();
        } else {
          ELOG_DEBUG("%s message: Updating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          video_transport_->getIceConnection()->setRemoteCredentials(username, password);
        }
      }
      if (!bundle_ && remote_sdp_->hasAudio) {
        std::string username = remote_sdp_->getUsername(AUDIO_TYPE);
        std::string password = remote_sdp_->getPassword(AUDIO_TYPE);
        if (audio_transport_.get() == nullptr) {
          ELOG_DEBUG("%s message: Creating audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audio_transport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, remote_sdp_->isRtcpMux,
                                                  listener, ice_config_, username, password, false,
                                                  worker_, io_worker_));
          audio_transport_->copyLogContextFrom(*this);
          audio_transport_->start();
        } else {
          ELOG_DEBUG("%s message: Update audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audio_transport_->getIceConnection()->setRemoteCredentials(username, password);
        }
      }
    }
  } else {
    ELOG_DEBUG("%s message: creating unencrypted transports", toLog());
    auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());
    if (bundle_) {
      std::string username = remote_sdp_->getUsername(VIDEO_TYPE);
      std::string password = remote_sdp_->getPassword(VIDEO_TYPE);
      if (video_transport_.get() == nullptr) {
        ELOG_DEBUG("%s message: Creating videoTransport, ufrag: %s, pass: %s",
                    toLog(), username.c_str(), password.c_str());
        video_transport_.reset(new UnencryptedTransport(VIDEO_TYPE, "video", connection_id_, bundle_,
                                                remote_sdp_->isRtcpMux, listener, ice_config_ , username, password,
                                                false, worker_, io_worker_));
        video_transport_->copyLogContextFrom(*this);
        video_transport_->start();
      } else {
        ELOG_DEBUG("%s message: Updating videoTransport, ufrag: %s, pass: %s",
                    toLog(), username.c_str(), password.c_str());
        video_transport_->getIceConnection()->setRemoteCredentials(username, password);
      }
    }
    if (!bundle_ && remote_sdp_->hasAudio) {
      std::string username = remote_sdp_->getUsername(AUDIO_TYPE);
      std::string password = remote_sdp_->getPassword(AUDIO_TYPE);
      if (audio_transport_.get() == nullptr) {
        ELOG_DEBUG("%s message: Creating audioTransport, ufrag: %s, pass: %s",
                    toLog(), username.c_str(), password.c_str());
        audio_transport_.reset(new UnencryptedTransport(AUDIO_TYPE, "audio", connection_id_, bundle_,
                                                remote_sdp_->isRtcpMux, listener, ice_config_, username, password,
                                                false, worker_, io_worker_));
        audio_transport_->copyLogContextFrom(*this);
        audio_transport_->start();
      } else {
        ELOG_DEBUG("%s message: Updating audioTransport, ufrag: %s, pass: %s",
                    toLog(), username.c_str(), password.c_str());
        audio_transport_->getIceConnection()->setRemoteCredentials(username, password);
      }
    }
  }
  if (this->getCurrentState() >= CONN_GATHERED) {
    if (!remote_sdp_->getCandidateInfos().empty()) {
      ELOG_DEBUG("%s message: Setting remote candidates after gathered", toLog());
      if (remote_sdp_->hasVideo) {
        video_transport_->setRemoteCandidates(remote_sdp_->getCandidateInfos(), bundle_);
      }
      if (!bundle_ && remote_sdp_->hasAudio) {
        audio_transport_->setRemoteCandidates(remote_sdp_->getCandidateInfos(), bundle_);
      }
    }
  }

  first_remote_sdp_processed_ = true;
  return setRemoteSdpsToMediaStreams();
}

boost::future<void> WebRtcConnection::addRemoteCandidate(std::string mid, int mLineIndex, CandidateInfo candidate) {
  return asyncTask([mid, mLineIndex, candidate] (std::shared_ptr<WebRtcConnection> connection) {
    connection->addRemoteCandidateSync(mid, mLineIndex, candidate);
  });
}

bool WebRtcConnection::addRemoteCandidateSync(std::string mid, int mLineIndex, CandidateInfo candidate) {
  std::vector<CandidateInfo> candidate_list;
  // TODO(pedro) Check type of transport.
  ELOG_DEBUG("%s message: Adding remote Candidate, candidate: %s, mid: %s, sdpMLine: %d",
              toLog(), candidate.to_string(), mid.c_str(), mLineIndex);
  if (video_transport_ == nullptr && audio_transport_ == nullptr) {
    ELOG_WARN("%s message: addRemoteCandidate on NULL transport", toLog());
    return false;
  }
  MediaType theType;

  if ((!mid.compare("video")) || (mLineIndex == remote_sdp_->videoSdpMLine)) {
    theType = VIDEO_TYPE;
  } else {
    theType = AUDIO_TYPE;
  }
  bool res = false;
  candidate_list.push_back(candidate);
  if (theType == VIDEO_TYPE || bundle_) {
    res = video_transport_->setRemoteCandidates(candidate_list, bundle_);
  } else if (theType == AUDIO_TYPE) {
    res = audio_transport_->setRemoteCandidates(candidate_list, bundle_);
  } else {
    ELOG_ERROR("%s message: add remote candidate with no Media (video or audio), candidate: %s",
                toLog(), candidate.to_string());
  }

  remote_sdp_->addCandidate(candidate);
  return res;
}

std::string WebRtcConnection::getJSONCandidate(const std::string& mid, const std::string& sdp) {
  std::map <std::string, std::string> object;
  object["sdpMid"] = mid;
  object["candidate"] = sdp;
  object["sdpMLineIndex"] =
  std::to_string((mid.compare("video")?local_sdp_->audioSdpMLine : local_sdp_->videoSdpMLine));

  std::ostringstream theString;
  theString << "{";
  for (std::map<std::string, std::string>::const_iterator it = object.begin(); it != object.end(); ++it) {
    theString << "\"" << it->first << "\":\"" << it->second << "\"";
    if (++it != object.end()) {
      theString << ",";
    }
    --it;
  }
  theString << "}";
  return theString.str();
}

void WebRtcConnection::maybeRestartIce(std::string username, std::string password) {
  asyncTask([username, password] (std::shared_ptr<WebRtcConnection> connection) {
    if (connection->video_transport_) {
      connection->video_transport_->maybeRestartIce(username, password);
    }
  });
}

void WebRtcConnection::onCandidate(const CandidateInfo& cand, Transport *transport) {
  std::string sdp = local_sdp_->addCandidate(cand);
  ELOG_DEBUG("%s message: Discovered New Candidate, candidate: %s, trickle_enabled %d", toLog(), sdp.c_str(),
  trickle_enabled_);
  if (trickle_enabled_) {
    if (!bundle_) {
      std::string object = this->getJSONCandidate(transport->transport_name, sdp);
      maybeNotifyWebRtcConnectionEvent(CONN_CANDIDATE, object);
    } else {
      if (remote_sdp_->hasAudio) {
        std::string object = this->getJSONCandidate("audio", sdp);
        maybeNotifyWebRtcConnectionEvent(CONN_CANDIDATE, object);
      }
      if (remote_sdp_->hasVideo) {
        std::string object2 = this->getJSONCandidate("video", sdp);
        maybeNotifyWebRtcConnectionEvent(CONN_CANDIDATE, object2);
      }
    }
  }
}

void WebRtcConnection::onREMBFromTransport(RtcpHeader *chead, Transport *transport) {
  std::vector<std::shared_ptr<MediaStream>> streams;

  for (uint8_t index = 0; index < chead->getREMBNumSSRC(); index++) {
    uint32_t ssrc_feed = chead->getREMBFeedSSRC(index);
    forEachMediaStream([ssrc_feed, &streams] (const std::shared_ptr<MediaStream> &media_stream) {
      if (media_stream->isSinkSSRC(ssrc_feed)) {
        streams.push_back(media_stream);
      }
    });
  }

  distributor_->distribute(chead->getREMBBitRate(), chead->getSSRC(), streams, transport);
}

void WebRtcConnection::onRtcpFromTransport(std::shared_ptr<DataPacket> packet, Transport *transport) {
  RtpUtils::forEachRtcpBlock(packet, [this, packet, transport](RtcpHeader *chead) {
    uint32_t ssrc = chead->isFeedback() ? chead->getSourceSSRC() : chead->getSSRC();
    if (chead->isREMB()) {
      onREMBFromTransport(chead, transport);
      return;
    }
    std::shared_ptr<DataPacket> rtcp = std::make_shared<DataPacket>(*packet);
    rtcp->length = (ntohs(chead->length) + 1) * 4;
    std::memcpy(rtcp->data, chead, rtcp->length);
    forEachMediaStream([rtcp, transport, ssrc] (const std::shared_ptr<MediaStream> &media_stream) {
      if (media_stream->isSourceSSRC(ssrc) || media_stream->isSinkSSRC(ssrc)) {
        media_stream->onTransportData(rtcp, transport);
      }
    });
  });
}

void WebRtcConnection::onTransportData(std::shared_ptr<DataPacket> packet, Transport *transport) {
  if (getCurrentState() != CONN_READY) {
    return;
  }
  if (transport->mediaType == AUDIO_TYPE) {
    packet->type = AUDIO_PACKET;
  } else if (transport->mediaType == VIDEO_TYPE) {
    packet->type = VIDEO_PACKET;
  }
  asyncTask([packet] (std::shared_ptr<WebRtcConnection> connection) {
    if (!connection->pipeline_initialized_) {
      ELOG_DEBUG("%s message: Pipeline not initialized yet.", connection->toLog());
      return;
    }

    if (connection->pipeline_) {
      connection->pipeline_->read(std::move(packet));
    }
  });
}

void WebRtcConnection::read(std::shared_ptr<DataPacket> packet) {
  Transport *transport = (bundle_ || packet->type == VIDEO_PACKET) ? video_transport_.get() : audio_transport_.get();
  if (transport == nullptr) {
    return;
  }
  char* buf = packet->data;
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
  if (chead->isRtcp()) {
    onRtcpFromTransport(packet, transport);
    return;
  } else {
    RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
    uint32_t ssrc = head->getSSRC();
    extension_processor_.processRtpExtensions(packet);
    std::string mid = packet->mid;
    extension_processor_.removeMidAndRidExtensions(packet);
    bool sent = false;
    forEachMediaStream([packet, transport, ssrc, &mid, &sent] (const std::shared_ptr<MediaStream> &media_stream) {
      if (!mid.empty()) {
        if (media_stream->getVideoMid() == mid || media_stream->getAudioMid() == mid) {
          sent = true;
          media_stream->onTransportData(packet, transport);
        }
      } else if (media_stream->isSourceSSRC(ssrc) || media_stream->isSinkSSRC(ssrc)) {
        sent = true;
        media_stream->onTransportData(packet, transport);
      }
    });
    if (!sent) {
      ELOG_DEBUG("Packet does not belong to a known stream, ssrc: %u, length: %d, mid: %s, rid: %s",
        ssrc, packet->length, packet->mid, packet->rid);
    }
  }
}

void WebRtcConnection::maybeNotifyWebRtcConnectionEvent(const WebRTCEvent& event, const std::string& message) {
  boost::mutex::scoped_lock lock(event_listener_mutex_);
  if (!conn_event_listener_) {
      return;
  }
  conn_event_listener_->notifyEvent(event, message);
}

boost::future<void> WebRtcConnection::asyncTask(
    std::function<void(std::shared_ptr<WebRtcConnection>)> f) {
  auto task_promise = std::make_shared<boost::promise<void>>();
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  worker_->task([weak_this, f, task_promise] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
    task_promise->set_value();
  });
  return task_promise->get_future();
}

void WebRtcConnection::updateState(TransportState state, Transport * transport) {
  boost::mutex::scoped_lock lock(update_state_mutex_);
  WebRTCEvent temp = global_state_;
  std::string msg = "";
  ELOG_DEBUG("%s transportName: %s, new_state: %d", toLog(), transport->transport_name.c_str(), state);
  if (video_transport_.get() == nullptr && audio_transport_.get() == nullptr) {
    ELOG_ERROR("%s message: Updating NULL transport, state: %d", toLog(), state);
    return;
  }
  if (global_state_ == CONN_FAILED) {
    // if current state is failed -> noop
    return;
  }
  switch (state) {
    case TRANSPORT_STARTED:
      if (bundle_) {
        temp = CONN_STARTED;
      } else {
        if ((!remote_sdp_->hasAudio || (audio_transport_.get() != nullptr
                  && audio_transport_->getTransportState() == TRANSPORT_STARTED)) &&
            (!remote_sdp_->hasVideo || (video_transport_.get() != nullptr
                  && video_transport_->getTransportState() == TRANSPORT_STARTED))) {
            // WebRTCConnection will be ready only when all channels are ready.
            temp = CONN_STARTED;
          }
      }
      break;
    case TRANSPORT_GATHERED:
      if (bundle_) {
        if (!remote_sdp_->getCandidateInfos().empty()) {
          // Passing now new candidates that could not be passed before
          if (remote_sdp_->hasVideo) {
            video_transport_->setRemoteCandidates(remote_sdp_->getCandidateInfos(), bundle_);
          }
          if (!bundle_ && remote_sdp_->hasAudio) {
            audio_transport_->setRemoteCandidates(remote_sdp_->getCandidateInfos(), bundle_);
          }
        }
        if (!trickle_enabled_) {
          temp = CONN_GATHERED;
          msg = "";
        }
      } else {
        if ((!local_sdp_->hasAudio || (audio_transport_.get() != nullptr
                  && audio_transport_->getTransportState() == TRANSPORT_GATHERED)) &&
            (!local_sdp_->hasVideo || (video_transport_.get() != nullptr
                  && video_transport_->getTransportState() == TRANSPORT_GATHERED))) {
            // WebRTCConnection will be ready only when all channels are ready.
            if (!trickle_enabled_) {
              temp = CONN_GATHERED;
              msg = "";
            }
          }
      }
      break;
    case TRANSPORT_READY:
      if (bundle_) {
        temp = CONN_READY;
        trackTransportInfo();
        forEachMediaStreamAsyncNoPromise([] (const std::shared_ptr<MediaStream> &media_stream) {
          media_stream->sendPLIToFeedback();
        });
      } else {
        if ((!remote_sdp_->hasAudio || (audio_transport_.get() != nullptr
                  && audio_transport_->getTransportState() == TRANSPORT_READY)) &&
            (!remote_sdp_->hasVideo || (video_transport_.get() != nullptr
                  && video_transport_->getTransportState() == TRANSPORT_READY))) {
            // WebRTCConnection will be ready only when all channels are ready.
            temp = CONN_READY;
            trackTransportInfo();
            forEachMediaStreamAsyncNoPromise([] (const std::shared_ptr<MediaStream> &media_stream) {
              media_stream->sendPLIToFeedback();
            });
          }
      }
      break;
    case TRANSPORT_FAILED:
      temp = CONN_FAILED;
      close();
      msg = "";
      ELOG_ERROR("%s message: Transport Failed, transportType: %s", toLog(), transport->transport_name.c_str() );
      break;
    default:
      ELOG_DEBUG("%s message: Doing nothing on state, state %d", toLog(), state);
      break;
  }

  if (audio_transport_.get() != nullptr && video_transport_.get() != nullptr) {
    ELOG_DEBUG("%s message: %s, transportName: %s, videoTransportState: %d"
              ", audioTransportState: %d, calculatedState: %d, globalState: %d",
      toLog(),
      "Update Transport State",
      transport->transport_name.c_str(),
      static_cast<int>(audio_transport_->getTransportState()),
      static_cast<int>(video_transport_->getTransportState()),
      static_cast<int>(temp),
      static_cast<int>(global_state_));
  }

  if (global_state_ == temp) {
    return;
  }

  global_state_ = temp;

  ELOG_INFO("%s newGlobalState: %d", toLog(), temp);
  maybeNotifyWebRtcConnectionEvent(global_state_, msg);
}

void WebRtcConnection::trackTransportInfo() {
  CandidatePair candidate_pair;
  std::string audio_info;
  std::string video_info;
  if (video_transport_ && video_transport_->getIceConnection()) {
    candidate_pair = video_transport_->getIceConnection()->getSelectedPair();
    video_info = candidate_pair.clientHostType;
  }

  if (audio_transport_ && video_transport_->getIceConnection()) {
    candidate_pair = audio_transport_->getIceConnection()->getSelectedPair();
    audio_info = candidate_pair.clientHostType;
  }

  asyncTask([audio_info, video_info] (std::shared_ptr<WebRtcConnection> connection) {
    connection->forEachMediaStreamAsyncNoPromise(
      [audio_info, video_info] (const std::shared_ptr<MediaStream> &media_stream) {
        media_stream->setTransportInfo(audio_info, video_info);
      });
  });
}

void WebRtcConnection::setMetadata(std::map<std::string, std::string> metadata) {
  for (const auto &item : metadata) {
    log_stats_->getNode().insertStat("metadata-" + item.first, StringStat{item.second});
  }
  setLogContext(metadata);
}

void WebRtcConnection::setBwDistributionConfigSync(BwDistributionConfig distribution_config) {
  ELOG_INFO("%s message: setting distribution config type %u", toLog(), distribution_config.selected_distributor);
  bw_distribution_config_ = distribution_config;
  forEachMediaStream([] (const std::shared_ptr<MediaStream> &media_stream) {
    media_stream->cleanPriorityState();
  });

  switch (distribution_config.selected_distributor) {
  case MAX_VIDEO_BW:
    distributor_ = std::unique_ptr<BandwidthDistributionAlgorithm>(new MaxVideoBWDistributor());
    break;
  case TARGET_VIDEO_BW:
    distributor_ = std::unique_ptr<BandwidthDistributionAlgorithm>(new TargetVideoBWDistributor());
    break;
  case STREAM_PRIORITY:
    distributor_ = std::unique_ptr<BandwidthDistributionAlgorithm>(
        new StreamPriorityBWDistributor(distribution_config.priority_strategy, stats_));
    break;
  }
}

void WebRtcConnection::setBwDistributionConfig(BwDistributionConfig distribution_config) {
  asyncTask([distribution_config](std::shared_ptr<WebRtcConnection> connection) {
    connection->setBwDistributionConfigSync(distribution_config);
  });
}

void WebRtcConnection::setWebRtcConnectionEventListener(WebRtcConnectionEventListener* listener) {
  boost::mutex::scoped_lock lock(event_listener_mutex_);
  this->conn_event_listener_ = listener;
}

WebRTCEvent WebRtcConnection::getCurrentState() {
  return global_state_;
}

void WebRtcConnection::send(std::shared_ptr<DataPacket> packet) {
  asyncTask([packet] (std::shared_ptr<WebRtcConnection> connection) {
    if (connection->pipeline_) {
      connection->pipeline_->write(std::move(packet));
    }
  });
}

void WebRtcConnection::write(std::shared_ptr<DataPacket> packet) {
  if (!sending_) {
    return;
  }
  Transport *transport = (bundle_ || packet->type == VIDEO_PACKET) ? video_transport_.get() : audio_transport_.get();
  if (transport == nullptr) {
    return;
  }
  extension_processor_.processRtpExtensions(packet);
  transport->write(packet->data, packet->length);
}

void WebRtcConnection::setTransport(std::shared_ptr<Transport> transport) {  // Only for Testing purposes
  video_transport_ = std::move(transport);
  bundle_ = true;
}

void WebRtcConnection::getJSONStats(std::function<void(std::string)> callback) {
  asyncTask([callback] (std::shared_ptr<WebRtcConnection> connection) {
    std::string requested_stats = connection->stats_->getStats();
    callback(requested_stats);
  });
}

}  // namespace erizo
