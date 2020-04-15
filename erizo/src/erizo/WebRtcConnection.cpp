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
#include "DtlsTransport.h"
#include "SdpInfo.h"
#include "bandwidth/MaxVideoBWDistributor.h"
#include "bandwidth/TargetVideoBWDistributor.h"
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
#include "rtp/RtpPaddingManagerHandler.h"
#include "rtp/RtpUtils.h"

namespace erizo {
DEFINE_LOGGER(WebRtcConnection, "WebRtcConnection");

WebRtcConnection::WebRtcConnection(std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker,
    const std::string& connection_id, const IceConfig& ice_config, const std::vector<RtpMap> rtp_mappings,
    const std::vector<erizo::ExtMap> ext_mappings, bool enable_connection_quality_check,
    WebRtcConnectionEventListener* listener) :
    connection_id_{connection_id},
    audio_enabled_{false}, video_enabled_{false}, bundle_{false}, conn_event_listener_{listener},
    ice_config_{ice_config}, rtp_mappings_{rtp_mappings}, extension_processor_{ext_mappings},
    worker_{worker}, io_worker_{io_worker},
    remote_sdp_{std::make_shared<SdpInfo>(rtp_mappings)}, local_sdp_{std::make_shared<SdpInfo>(rtp_mappings)},
    audio_muted_{false}, video_muted_{false}, first_remote_sdp_processed_{false},
    enable_connection_quality_check_{enable_connection_quality_check}, pipeline_{Pipeline::create()},
    pipeline_initialized_{false} {
  ELOG_INFO("%s message: constructor, stunserver: %s, stunPort: %d, minPort: %d, maxPort: %d",
      toLog(), ice_config.stun_server.c_str(), ice_config.stun_port, ice_config.min_port, ice_config.max_port);
  stats_ = std::make_shared<Stats>();
  distributor_ = std::unique_ptr<BandwidthDistributionAlgorithm>(new TargetVideoBWDistributor());
  global_state_ = CONN_INITIAL;

  trickle_enabled_ = ice_config_.should_trickle;
  slide_show_mode_ = false;

  sending_ = true;
}

WebRtcConnection::~WebRtcConnection() {
  ELOG_DEBUG("%s message:Destructor called", toLog());
  ELOG_DEBUG("%s message: Destructor ended", toLog());
}

void WebRtcConnection::syncClose() {
  ELOG_DEBUG("%s message: Close called", toLog());
  if (!sending_) {
    return;
  }
  sending_ = false;
  media_streams_.clear();
  if (video_transport_.get()) {
    video_transport_->close();
  }
  if (audio_transport_.get()) {
    audio_transport_->close();
  }
  global_state_ = CONN_FINISHED;
  if (conn_event_listener_ != nullptr) {
    conn_event_listener_ = nullptr;
  }
  pipeline_initialized_ = false;
  pipeline_->close();
  pipeline_.reset();

  ELOG_DEBUG("%s message: Close ended", toLog());
}

void WebRtcConnection::close() {
  ELOG_DEBUG("%s message: Async close called", toLog());
  std::shared_ptr<WebRtcConnection> shared_this = shared_from_this();
  asyncTask([shared_this] (std::shared_ptr<WebRtcConnection> connection) {
    shared_this->syncClose();
  });
}

bool WebRtcConnection::init() {
  asyncTask([] (std::shared_ptr<WebRtcConnection> connection) {
    connection->initializePipeline();
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

  pipeline_->addFront(std::make_shared<SenderBandwidthEstimationHandler>());
  pipeline_->addFront(std::make_shared<RtpPaddingManagerHandler>());

  pipeline_->addFront(std::make_shared<ConnectionPacketWriter>(this));
  pipeline_->finalize();
  pipeline_initialized_ = true;
}

void WebRtcConnection::notifyUpdateToHandlers() {
  asyncTask([] (std::shared_ptr<WebRtcConnection> conn) {
    if (conn && conn->pipeline_ && conn->pipeline_initialized_) {
      conn->pipeline_->notifyUpdate();
    }
  });
}

boost::future<void> WebRtcConnection::createOffer(bool video_enabled, bool audio_enabled, bool bundle) {
  return asyncTask([video_enabled, audio_enabled, bundle] (std::shared_ptr<WebRtcConnection> connection) {
    connection->createOfferSync(video_enabled, audio_enabled, bundle);
  });
}

bool WebRtcConnection::createOfferSync(bool video_enabled, bool audio_enabled, bool bundle) {
  boost::mutex::scoped_lock lock(update_state_mutex_);
  bundle_ = bundle;
  video_enabled_ = video_enabled;
  audio_enabled_ = audio_enabled;
  local_sdp_->createOfferSdp(video_enabled_, audio_enabled_, bundle_);

  local_sdp_->dtlsRole = ACTPASS;
  if (local_sdp_->internal_dtls_role == ACTPASS) {
    local_sdp_->internal_dtls_role = PASSIVE;
  }

  ELOG_DEBUG("%s message: Creating sdp offer, isBundle: %d, setup: %d",
    toLog(), bundle_, local_sdp_->internal_dtls_role);

  forEachMediaStream([this] (const std::shared_ptr<MediaStream> &media_stream) {
    if (!media_stream->isReady() || media_stream->isPublisher()) {
      ELOG_DEBUG("%s message: getting local SDPInfo stream not running, stream_id: %s", toLog(), media_stream->getId());
      return;
    }
    if (video_enabled_) {
      std::vector<uint32_t> video_ssrc_list = std::vector<uint32_t>();
      if (media_stream->getVideoSinkSSRC() != kDefaultVideoSinkSSRC && media_stream->getVideoSinkSSRC() != 0) {
        video_ssrc_list.push_back(media_stream->getVideoSinkSSRC());
      }
      ELOG_DEBUG("%s message: getting local SDPInfo, stream_id: %s, audio_ssrc: %u",
                 toLog(), media_stream->getId(), media_stream->getAudioSinkSSRC());
      if (!video_ssrc_list.empty()) {
        local_sdp_->video_ssrc_map[media_stream->getLabel()] = video_ssrc_list;
      }
    }
    if (audio_enabled_) {
      if (media_stream->getAudioSinkSSRC() != kDefaultAudioSinkSSRC && media_stream->getAudioSinkSSRC() != 0) {
        local_sdp_->audio_ssrc_map[media_stream->getLabel()] = media_stream->getAudioSinkSSRC();
      }
    }
  });

  auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());

  if (bundle_) {
    if (video_transport_.get() == nullptr && (video_enabled_ || audio_enabled_)) {
      video_transport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              listener, ice_config_ , "", "", true, worker_, io_worker_));
      video_transport_->copyLogContextFrom(*this);
      video_transport_->start();
    }
  } else {
    if (video_transport_.get() == nullptr && video_enabled_) {
      // For now we don't re/check transports, if they are already created we leave them there
      video_transport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              listener, ice_config_ , "", "", true, worker_, io_worker_));
      video_transport_->copyLogContextFrom(*this);
      video_transport_->start();
    }
    if (audio_transport_.get() == nullptr && audio_enabled_) {
      audio_transport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, true,
                                              listener, ice_config_, "", "", true, worker_, io_worker_));
      audio_transport_->copyLogContextFrom(*this);
      audio_transport_->start();
    }
  }
  std::string msg = "";
  maybeNotifyWebRtcConnectionEvent(global_state_, msg);

  return true;
}

ConnectionQualityLevel WebRtcConnection::getConnectionQualityLevel() {
  return connection_quality_check_.getLevel();
}

bool WebRtcConnection::werePacketLossesRecently() {
  return connection_quality_check_.werePacketLossesRecently();
}

boost::future<void> WebRtcConnection::addMediaStream(std::shared_ptr<MediaStream> media_stream) {
  return asyncTask([media_stream] (std::shared_ptr<WebRtcConnection> connection) {
    boost::mutex::scoped_lock lock(connection->update_state_mutex_);
    ELOG_DEBUG("%s message: Adding mediaStream, id: %s", connection->toLog(), media_stream->getId().c_str());
    connection->media_streams_.push_back(media_stream);
  });
}

boost::future<void> WebRtcConnection::removeMediaStream(const std::string& stream_id) {
  return asyncTask([stream_id] (std::shared_ptr<WebRtcConnection> connection) {
    boost::mutex::scoped_lock lock(connection->update_state_mutex_);
    ELOG_DEBUG("%s message: removing mediaStream, id: %s", connection->toLog(), stream_id.c_str());
    connection->media_streams_.erase(std::remove_if(connection->media_streams_.begin(),
                                                    connection->media_streams_.end(),
      [stream_id, connection](const std::shared_ptr<MediaStream> &stream) {
        bool isStream = stream->getId() == stream_id;
        if (isStream) {
          auto video_it = connection->local_sdp_->video_ssrc_map.find(stream->getLabel());
          if (video_it != connection->local_sdp_->video_ssrc_map.end()) {
            connection->local_sdp_->video_ssrc_map.erase(video_it);
          }
          auto audio_it = connection->local_sdp_->audio_ssrc_map.find(stream->getLabel());
          if (audio_it != connection->local_sdp_->audio_ssrc_map.end()) {
            connection->local_sdp_->audio_ssrc_map.erase(audio_it);
          }
        }
        return isStream;
      }));
    });
}

void WebRtcConnection::forEachMediaStream(std::function<void(const std::shared_ptr<MediaStream>&)> func) {
  std::for_each(media_streams_.begin(), media_streams_.end(), func);
}

boost::future<void> WebRtcConnection::forEachMediaStreamAsync(
    std::function<void(const std::shared_ptr<MediaStream>&)> func) {
  auto futures = std::make_shared<std::vector<boost::future<void>>>();
  std::for_each(media_streams_.begin(), media_streams_.end(),
    [func, futures] (const std::shared_ptr<MediaStream> &stream) {
      futures->push_back(stream->asyncTask([func] (const std::shared_ptr<MediaStream> &stream) {
        func(stream);
      }));
  });

  auto future_when = boost::when_all(futures->begin(), futures->end());
  return future_when.then([](decltype(future_when)) {
    });
}

void WebRtcConnection::forEachMediaStreamAsyncNoPromise(
    std::function<void(const std::shared_ptr<MediaStream>&)> func) {
  std::for_each(media_streams_.begin(), media_streams_.end(),
    [func] (const std::shared_ptr<MediaStream> &stream) {
      stream->asyncTask([func] (const std::shared_ptr<MediaStream> &stream) {
        func(stream);
      });
  });
}

boost::future<void> WebRtcConnection::setRemoteSdpInfo(
    std::shared_ptr<SdpInfo> sdp, int received_session_version) {
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  auto task_promise = std::make_shared<boost::promise<void>>();
  worker_->task([weak_this, sdp, task_promise, received_session_version] {
    if (auto connection = weak_this.lock()) {
      ELOG_DEBUG("%s message: setting remote SDPInfo", connection->toLog());
      if (!connection->sending_) {
        task_promise->set_value();
        return;
      }
      connection->remote_sdp_ = sdp;
      connection->notifyUpdateToHandlers();
      boost::future<void> future = connection->processRemoteSdp(received_session_version).then(
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
    if (connection->sending_ && !connection->first_remote_sdp_processed_) {
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

std::shared_ptr<SdpInfo> WebRtcConnection::getLocalSdpInfoSync() {
  boost::mutex::scoped_lock lock(update_state_mutex_);
  ELOG_DEBUG("%s message: getting local SDPInfo", toLog());
  forEachMediaStream([this] (const std::shared_ptr<MediaStream> &media_stream) {
    if (!media_stream->isReady() || media_stream->isPublisher()) {
      ELOG_DEBUG("%s message: getting local SDPInfo stream not running, stream_id: %s", toLog(), media_stream->getId());
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

  audio_enabled_ = sending_audio || receiving_audio;
  video_enabled_ = sending_video || receiving_video;

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

  if (video_transport_ != nullptr && getCurrentState() != CONN_READY) {
    video_transport_->processLocalSdp(local_sdp_.get());
  }
  if (!bundle_ && audio_transport_ != nullptr && getCurrentState() != CONN_READY) {
    audio_transport_->processLocalSdp(local_sdp_.get());
  }
  local_sdp_->profile = remote_sdp_->profile;

  auto local_sdp_copy = std::make_shared<SdpInfo>(*local_sdp_.get());
  return local_sdp_copy;
}

boost::future<void> WebRtcConnection::setRemoteSdpsToMediaStreams(int received_session_version) {
  ELOG_DEBUG("%s message: setting remote SDP, streams: %d", toLog(), media_streams_.size());
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  std::shared_ptr<SdpInfo> remote_sdp = std::make_shared<SdpInfo>(*remote_sdp_.get());
  return forEachMediaStreamAsync([weak_this, remote_sdp, received_session_version]
      (std::shared_ptr<MediaStream> media_stream) {
    if (auto connection = weak_this.lock()) {
      media_stream->setRemoteSdp(remote_sdp, received_session_version);
      ELOG_DEBUG("%s message: setting remote SDP to stream, stream: %s",
        connection->toLog(), media_stream->getId());
    }
  });
}

boost::future<void> WebRtcConnection::processRemoteSdp(int received_session_version = -1) {
  ELOG_DEBUG("%s message: processing remote SDP", toLog());
  if (!first_remote_sdp_processed_ && local_sdp_->internal_dtls_role == ACTPASS) {
    local_sdp_->internal_dtls_role = ACTIVE;
  }
  local_sdp_->dtlsRole = local_sdp_->internal_dtls_role;
  ELOG_DEBUG("%s message: process remote sdp, setup: %d", toLog(), local_sdp_->internal_dtls_role);

  if (first_remote_sdp_processed_) {
    return setRemoteSdpsToMediaStreams(received_session_version);
  }

  bundle_ = remote_sdp_->isBundle;
  local_sdp_->setOfferSdp(remote_sdp_);
  extension_processor_.setSdpInfo(local_sdp_);
  local_sdp_->updateSupportedExtensionMap(extension_processor_.getSupportedExtensionMap());


  audio_enabled_ = remote_sdp_->hasAudio;
  video_enabled_ = remote_sdp_->hasVideo;

  if (remote_sdp_->profile == SAVPF) {
    if (remote_sdp_->isFingerprint) {
      auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());
      if (remote_sdp_->hasVideo || bundle_) {
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
  return setRemoteSdpsToMediaStreams(received_session_version);
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
  std::string theMid;

  // TODO(pedro) check if this works with video+audio and no bundle
  if (mLineIndex == -1) {
    ELOG_DEBUG("%s message: All candidates received", toLog());
    if (video_transport_) {
      video_transport_->getIceConnection()->setReceivedLastCandidate(true);
    } else if (audio_transport_) {
      audio_transport_->getIceConnection()->setReceivedLastCandidate(true);
    }
    return true;
  }

  if ((!mid.compare("video")) || (mLineIndex == remote_sdp_->videoSdpMLine)) {
    theType = VIDEO_TYPE;
    theMid = "video";
  } else {
    theType = AUDIO_TYPE;
    theMid = "audio";
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

void WebRtcConnection::onCandidate(const CandidateInfo& cand, Transport *transport) {
  std::string sdp = local_sdp_->addCandidate(cand);
  ELOG_DEBUG("%s message: Discovered New Candidate, candidate: %s", toLog(), sdp.c_str());
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
  if (enable_connection_quality_check_) {
    connection_quality_check_.onFeedback(packet, media_streams_);
  }
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
    forEachMediaStream([packet, transport, ssrc] (const std::shared_ptr<MediaStream> &media_stream) {
      if (media_stream->isSourceSSRC(ssrc) || media_stream->isSinkSSRC(ssrc)) {
        media_stream->onTransportData(packet, transport);
      }
    });
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
      sending_ = false;
      msg = "";
      ELOG_ERROR("%s message: Transport Failed, transportType: %s", toLog(), transport->transport_name.c_str() );
      cond_.notify_one();
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
  if (video_enabled_ && video_transport_) {
    candidate_pair = video_transport_->getIceConnection()->getSelectedPair();
    video_info = candidate_pair.clientHostType;
  }

  if (audio_enabled_ && audio_transport_) {
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
  setLogContext(metadata);
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
  this->extension_processor_.processRtpExtensions(packet);
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
