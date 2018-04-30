/*
 * WebRTCConnection.cpp
 */

#include <cstdio>
#include <map>
#include <algorithm>
#include <string>
#include <cstring>
#include <vector>

#include "WebRtcConnection.h"
#include "MediaStream.h"
#include "DtlsTransport.h"
#include "SdpInfo.h"
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

namespace erizo {
DEFINE_LOGGER(WebRtcConnection, "WebRtcConnection");

WebRtcConnection::WebRtcConnection(std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker,
    const std::string& connection_id, const IceConfig& ice_config, const std::vector<RtpMap> rtp_mappings,
    const std::vector<erizo::ExtMap> ext_mappings, WebRtcConnectionEventListener* listener) :
    connection_id_{connection_id},
    audio_enabled_{false}, video_enabled_{false}, bundle_{false}, conn_event_listener_{listener},
    ice_config_{ice_config}, rtp_mappings_{rtp_mappings}, extension_processor_{ext_mappings},
    worker_{worker}, io_worker_{io_worker},
    remote_sdp_{std::make_shared<SdpInfo>(rtp_mappings)}, local_sdp_{std::make_shared<SdpInfo>(rtp_mappings)},
    audio_muted_{false}, video_muted_{false}, first_remote_sdp_processed_{false}
    {
  ELOG_INFO("%s message: constructor, stunserver: %s, stunPort: %d, minPort: %d, maxPort: %d",
      toLog(), ice_config.stun_server.c_str(), ice_config.stun_port, ice_config.min_port, ice_config.max_port);
  stats_ = std::make_shared<Stats>();
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
    maybeNotifyWebRtcConnectionEvent(global_state_, "");
    return true;
}

bool WebRtcConnection::createOffer(bool video_enabled, bool audioEnabled, bool bundle) {
  boost::mutex::scoped_lock lock(update_state_mutex_);
  bundle_ = bundle;
  video_enabled_ = video_enabled;
  audio_enabled_ = audioEnabled;
  local_sdp_->createOfferSdp(video_enabled_, audio_enabled_, bundle_);
  local_sdp_->dtlsRole = ACTPASS;

  ELOG_DEBUG("%s message: Creating sdp offer, isBundle: %d", toLog(), bundle_);

  if (video_enabled_) {
    forEachMediaStream([this] (const std::shared_ptr<MediaStream> &media_stream) {
      std::vector<uint32_t> video_ssrc_list = std::vector<uint32_t>();
      video_ssrc_list.push_back(media_stream->getVideoSinkSSRC());
      local_sdp_->video_ssrc_map[media_stream->getLabel()] = video_ssrc_list;
    });
  }
  if (audio_enabled_) {
    forEachMediaStream([this] (const std::shared_ptr<MediaStream> &media_stream) {
      local_sdp_->audio_ssrc_map[media_stream->getLabel()] = media_stream->getAudioSinkSSRC();
    });
  }


  auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());

  if (bundle_) {
    video_transport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                            listener, ice_config_ , "", "", true, worker_, io_worker_));
    video_transport_->copyLogContextFrom(this);
    video_transport_->start();
  } else {
    if (video_transport_.get() == nullptr && video_enabled_) {
      // For now we don't re/check transports, if they are already created we leave them there
      video_transport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              listener, ice_config_ , "", "", true, worker_, io_worker_));
      video_transport_->copyLogContextFrom(this);
      video_transport_->start();
    }
    if (audio_transport_.get() == nullptr && audio_enabled_) {
      audio_transport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, true,
                                              listener, ice_config_, "", "", true, worker_, io_worker_));
      audio_transport_->copyLogContextFrom(this);
      audio_transport_->start();
    }
  }

  std::string msg = this->getLocalSdp();
  maybeNotifyWebRtcConnectionEvent(global_state_, msg);

  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  forEachMediaStreamAsync([weak_this] (const std::shared_ptr<MediaStream> &media_stream) {
    if (auto connection = weak_this.lock()) {
      media_stream->setLocalSdp(connection->local_sdp_);
    }
  });
  return true;
}

void WebRtcConnection::addMediaStream(std::shared_ptr<MediaStream> media_stream) {
  asyncTask([media_stream] (std::shared_ptr<WebRtcConnection> connection) {
    ELOG_DEBUG("%s message: Adding mediaStream, id: %s", connection->toLog(), media_stream->getId().c_str());
    connection->media_streams_.push_back(media_stream);
  });
}

void WebRtcConnection::removeMediaStream(const std::string& stream_id) {
  asyncTask([stream_id] (std::shared_ptr<WebRtcConnection> connection) {
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

void WebRtcConnection::forEachMediaStreamAsync(std::function<void(const std::shared_ptr<MediaStream>&)> func) {
  std::for_each(media_streams_.begin(), media_streams_.end(),
    [func] (const std::shared_ptr<MediaStream> &stream) {
    stream->asyncTask([func] (const std::shared_ptr<MediaStream> &stream) {
      func(stream);
    });
  });
}

bool WebRtcConnection::setRemoteSdpInfo(std::shared_ptr<SdpInfo> sdp, std::string stream_id) {
  asyncTask([sdp, stream_id] (std::shared_ptr<WebRtcConnection> connection) {
    ELOG_DEBUG("%s message: setting remote SDPInfo", connection->toLog());

    if (!connection->sending_) {
      return;
    }

    connection->remote_sdp_ = sdp;
    connection->processRemoteSdp(stream_id);
  });
  return true;
}

std::shared_ptr<SdpInfo> WebRtcConnection::getLocalSdpInfo() {
  boost::mutex::scoped_lock lock(update_state_mutex_);
  ELOG_DEBUG("%s message: getting local SDPInfo", toLog());
  forEachMediaStream([this] (const std::shared_ptr<MediaStream> &media_stream) {
    if (!media_stream->isRunning() || media_stream->isPublisher()) {
      ELOG_DEBUG("%s message: getting local SDPInfo stream not running, stream_id: %s", toLog(), media_stream->getId());
      return;
    }
    std::vector<uint32_t> video_ssrc_list = std::vector<uint32_t>();
    if (media_stream->getVideoSinkSSRC() != kDefaultVideoSinkSSRC) {
      video_ssrc_list.push_back(media_stream->getVideoSinkSSRC());
    }
    ELOG_DEBUG("%s message: getting local SDPInfo, stream_id: %s, audio_ssrc: %u",
               toLog(), media_stream->getId(), media_stream->getAudioSinkSSRC());
    if (!video_ssrc_list.empty()) {
      local_sdp_->video_ssrc_map[media_stream->getLabel()] = video_ssrc_list;
    }
    if (media_stream->getAudioSinkSSRC() != kDefaultAudioSinkSSRC) {
      local_sdp_->audio_ssrc_map[media_stream->getLabel()] = media_stream->getAudioSinkSSRC();
    }
  });

  bool sending_audio = local_sdp_->audio_ssrc_map.size() > 0;
  bool sending_video = local_sdp_->video_ssrc_map.size() > 0;

  bool receiving_audio = remote_sdp_->audio_ssrc_map.size() > 0;
  bool receiving_video = remote_sdp_->video_ssrc_map.size() > 0;

  if (!sending_audio && receiving_audio) {
    local_sdp_->audioDirection = erizo::RECVONLY;
  } else if (sending_audio && !receiving_audio) {
    local_sdp_->audioDirection = erizo::SENDONLY;
  } else {
    local_sdp_->audioDirection = erizo::SENDRECV;
  }

  if (!sending_video && receiving_video) {
    local_sdp_->videoDirection = erizo::RECVONLY;
  } else if (sending_video && !receiving_video) {
    local_sdp_->videoDirection = erizo::SENDONLY;
  } else {
    local_sdp_->videoDirection = erizo::SENDRECV;
  }

  return local_sdp_;
}

bool WebRtcConnection::setRemoteSdp(const std::string &sdp, std::string stream_id) {
  asyncTask([sdp, stream_id] (std::shared_ptr<WebRtcConnection> connection) {
    ELOG_DEBUG("%s message: setting remote SDP", connection->toLog());
    if (!connection->sending_) {
      return;
    }

    connection->remote_sdp_->initWithSdp(sdp, "");
    connection->processRemoteSdp(stream_id);
  });
  return true;
}

void WebRtcConnection::setRemoteSdpsToMediaStreams(std::string stream_id) {
  ELOG_DEBUG("%s message: setting remote SDP, stream: %s", toLog(), stream_id);

  auto stream = std::find_if(media_streams_.begin(), media_streams_.end(),
    [stream_id, this](const std::shared_ptr<MediaStream> &media_stream) {
      ELOG_DEBUG("%s message: setting remote SDP, stream: %s, stream_id: %s",
        toLog(), media_stream->getId(), stream_id);
      return media_stream->getId() == stream_id;
    });

  if (stream != media_streams_.end()) {
    std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
    (*stream)->asyncTask([weak_this, stream_id] (const std::shared_ptr<MediaStream> &media_stream) {
      if (auto connection = weak_this.lock()) {
        media_stream->setRemoteSdp(connection->remote_sdp_);
        media_stream->setLocalSdp(connection->local_sdp_);
        ELOG_DEBUG("%s message: setting remote SDP to stream, stream: %s", connection->toLog(), media_stream->getId());
        connection->onRemoteSdpsSetToMediaStreams(stream_id);
      }
    });
  } else {
    onRemoteSdpsSetToMediaStreams(stream_id);
  }
}

void WebRtcConnection::onRemoteSdpsSetToMediaStreams(std::string stream_id) {
  asyncTask([stream_id] (std::shared_ptr<WebRtcConnection> connection) {
    ELOG_DEBUG("%s message: SDP processed", connection->toLog());
    std::string sdp = connection->getLocalSdp();
    connection->maybeNotifyWebRtcConnectionEvent(CONN_SDP_PROCESSED, sdp, stream_id);
  });
}

bool WebRtcConnection::processRemoteSdp(std::string stream_id) {
  ELOG_DEBUG("%s message: processing remote SDP", toLog());
  if (first_remote_sdp_processed_) {
    setRemoteSdpsToMediaStreams(stream_id);
    return true;
  }

  bundle_ = remote_sdp_->isBundle;
  local_sdp_->setOfferSdp(remote_sdp_);
  extension_processor_.setSdpInfo(local_sdp_);
  local_sdp_->updateSupportedExtensionMap(extension_processor_.getSupportedExtensionMap());

  if (remote_sdp_->dtlsRole == ACTPASS) {
    local_sdp_->dtlsRole = ACTIVE;
  }

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
          video_transport_->copyLogContextFrom(this);
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
          audio_transport_->copyLogContextFrom(this);
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
  setRemoteSdpsToMediaStreams(stream_id);
  first_remote_sdp_processed_ = true;
  return true;
}


bool WebRtcConnection::addRemoteCandidate(const std::string &mid, int mLineIndex, const std::string &sdp) {
  // TODO(pedro) Check type of transport.
  ELOG_DEBUG("%s message: Adding remote Candidate, candidate: %s, mid: %s, sdpMLine: %d",
              toLog(), sdp.c_str(), mid.c_str(), mLineIndex);
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
  SdpInfo tempSdp(rtp_mappings_);
  std::string username = remote_sdp_->getUsername(theType);
  std::string password = remote_sdp_->getPassword(theType);
  tempSdp.setCredentials(username, password, OTHER);
  bool res = false;
  if (tempSdp.initWithSdp(sdp, theMid)) {
    if (theType == VIDEO_TYPE || bundle_) {
      res = video_transport_->setRemoteCandidates(tempSdp.getCandidateInfos(), bundle_);
    } else if (theType == AUDIO_TYPE) {
      res = audio_transport_->setRemoteCandidates(tempSdp.getCandidateInfos(), bundle_);
    } else {
      ELOG_ERROR("%s message: add remote candidate with no Media (video or audio), candidate: %s",
                  toLog(), sdp.c_str() );
    }
  }

  for (uint8_t it = 0; it < tempSdp.getCandidateInfos().size(); it++) {
    remote_sdp_->addCandidate(tempSdp.getCandidateInfos()[it]);
  }
  return res;
}

std::string WebRtcConnection::getLocalSdp() {
  ELOG_DEBUG("%s message: Getting Local Sdp", toLog());
  if (video_transport_ != nullptr && getCurrentState() != CONN_READY) {
    video_transport_->processLocalSdp(local_sdp_.get());
  }
  if (!bundle_ && audio_transport_ != nullptr && getCurrentState() != CONN_READY) {
    audio_transport_->processLocalSdp(local_sdp_.get());
  }
  local_sdp_->profile = remote_sdp_->profile;
  return local_sdp_->getSdp();
}

std::string WebRtcConnection::getJSONCandidate(const std::string& mid, const std::string& sdp) {
  std::map <std::string, std::string> object;
  object["sdpMid"] = mid;
  object["candidate"] = sdp;
  object["sdpMLineIndex"] =
  std::to_string((mid.compare("video")?local_sdp_->audioSdpMLine : local_sdp_->videoSdpMLine));

  std::ostringstream theString;
  theString << "{";
  for (std::map<std::string, std::string>::iterator it = object.begin(); it != object.end(); ++it) {
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

  std::sort(streams.begin(), streams.end(),
    [](const std::shared_ptr<MediaStream> &i, const std::shared_ptr<MediaStream> &j) {
      return i->getMaxVideoBW() < j->getMaxVideoBW();
    });

  uint8_t remaining_streams = streams.size();
  uint32_t remaining_bitrate = chead->getREMBBitRate();
  std::for_each(streams.begin(), streams.end(),
    [&remaining_bitrate, &remaining_streams, transport, chead](const std::shared_ptr<MediaStream> &stream) {
      uint32_t max_bitrate = stream->getMaxVideoBW();
      uint32_t remaining_avg_bitrate = remaining_bitrate / remaining_streams;
      uint32_t bitrate = std::min(max_bitrate, remaining_avg_bitrate);
      auto generated_remb = RtpUtils::createREMB(chead->getSSRC(), {stream->getVideoSinkSSRC()}, bitrate);
      stream->onTransportData(generated_remb, transport);
      remaining_bitrate -= bitrate;
      remaining_streams--;
    });
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

void WebRtcConnection::maybeNotifyWebRtcConnectionEvent(const WebRTCEvent& event, const std::string& message,
    const std::string& stream_id) {
  boost::mutex::scoped_lock lock(event_listener_mutex_);
  if (!conn_event_listener_) {
      return;
  }
  conn_event_listener_->notifyEvent(event, message, stream_id);
}

void WebRtcConnection::asyncTask(std::function<void(std::shared_ptr<WebRtcConnection>)> f) {
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  worker_->task([weak_this, f] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
  });
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
          msg = this->getLocalSdp();
        }
      } else {
        if ((!local_sdp_->hasAudio || (audio_transport_.get() != nullptr
                  && audio_transport_->getTransportState() == TRANSPORT_GATHERED)) &&
            (!local_sdp_->hasVideo || (video_transport_.get() != nullptr
                  && video_transport_->getTransportState() == TRANSPORT_GATHERED))) {
            // WebRTCConnection will be ready only when all channels are ready.
            if (!trickle_enabled_) {
              temp = CONN_GATHERED;
              msg = this->getLocalSdp();
            }
          }
      }
      break;
    case TRANSPORT_READY:
      if (bundle_) {
        temp = CONN_READY;
        trackTransportInfo();
        forEachMediaStreamAsync([] (const std::shared_ptr<MediaStream> &media_stream) {
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
            forEachMediaStreamAsync([] (const std::shared_ptr<MediaStream> &media_stream) {
              media_stream->sendPLIToFeedback();
            });
          }
      }
      break;
    case TRANSPORT_FAILED:
      temp = CONN_FAILED;
      sending_ = false;
      msg = remote_sdp_->getSdp();
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

  ELOG_INFO("%s newGlobalState: %d", toLog(), global_state_);
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
    connection->forEachMediaStreamAsync(
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

void WebRtcConnection::write(std::shared_ptr<DataPacket> packet) {
  asyncTask([packet] (std::shared_ptr<WebRtcConnection> connection) {
    connection->syncWrite(packet);
  });
}

void WebRtcConnection::syncWrite(std::shared_ptr<DataPacket> packet) {
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
  video_transport_ = transport;
  bundle_ = true;
}

}  // namespace erizo
