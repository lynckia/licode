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
    const std::string& connection_id, const IceConfig& iceConfig, const std::vector<RtpMap> rtp_mappings,
    const std::vector<erizo::ExtMap> ext_mappings, WebRtcConnectionEventListener* listener) :
    connection_id_{connection_id},
    audioEnabled_{false}, videoEnabled_{false}, bundle_{false}, connEventListener_{listener},
    iceConfig_{iceConfig}, rtp_mappings_{rtp_mappings}, extProcessor_{ext_mappings},
    worker_{worker}, io_worker_{io_worker},
    remote_sdp_{std::make_shared<SdpInfo>(rtp_mappings)}, local_sdp_{std::make_shared<SdpInfo>(rtp_mappings)},
    audio_muted_{false}, video_muted_{false}
    {
  ELOG_INFO("%s message: constructor, stunserver: %s, stunPort: %d, minPort: %d, maxPort: %d",
      toLog(), iceConfig.stun_server.c_str(), iceConfig.stun_port, iceConfig.min_port, iceConfig.max_port);
  stats_ = std::make_shared<Stats>();
  globalState_ = CONN_INITIAL;

  trickleEnabled_ = iceConfig_.should_trickle;
  shouldSendFeedback_ = true;
  slide_show_mode_ = false;
  mark_ = clock::now();

  rateControl_ = 0;
  sending_ = true;
}

WebRtcConnection::~WebRtcConnection() {
  ELOG_DEBUG("%s message:Destructor called", toLog());
  if (sending_) {
    syncClose();
  }
  ELOG_DEBUG("%s message: Destructor ended", toLog());
}

void WebRtcConnection::syncClose() {
  ELOG_DEBUG("%s message: Close called", toLog());
  if (!sending_) {
    return;
  }
  sending_ = false;
  if (videoTransport_.get()) {
    videoTransport_->close();
  }
  if (audioTransport_.get()) {
    audioTransport_->close();
  }
  globalState_ = CONN_FINISHED;
  if (connEventListener_ != nullptr) {
    connEventListener_ = nullptr;
  }

  ELOG_DEBUG("%s message: Close ended", toLog());
}

void WebRtcConnection::close() {
  ELOG_DEBUG("%s message: Async close called", toLog());
  asyncTask([] (std::shared_ptr<WebRtcConnection> connection) {
    connection->syncClose();
  });

}

bool WebRtcConnection::init() {
  if (connEventListener_ != nullptr) {
    connEventListener_->notifyEvent(globalState_, "");
  }
  return true;
}

bool WebRtcConnection::isSourceSSRC(uint32_t ssrc) {
  return media_stream_->isVideoSourceSSRC(ssrc) || media_stream_->isAudioSourceSSRC(ssrc);
}

bool WebRtcConnection::isSinkSSRC(uint32_t ssrc) {
  return media_stream_->isVideoSinkSSRC(ssrc) || media_stream_->isAudioSinkSSRC(ssrc);
}

bool WebRtcConnection::createOffer(bool videoEnabled, bool audioEnabled, bool bundle) {
  bundle_ = bundle;
  videoEnabled_ = videoEnabled;
  audioEnabled_ = audioEnabled;
  local_sdp_->createOfferSdp(videoEnabled_, audioEnabled_, bundle_);
  local_sdp_->dtlsRole = ACTPASS;

  ELOG_DEBUG("%s message: Creating sdp offer, isBundle: %d", toLog(), bundle_);

  if (videoEnabled_)
    local_sdp_->video_ssrc_list.push_back(media_stream_->getVideoSinkSSRC());
  if (audioEnabled_)
    local_sdp_->audio_ssrc = media_stream_->getAudioSinkSSRC();

  auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());

  if (bundle_) {
    videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                            listener, iceConfig_ , "", "", true, worker_, io_worker_));
    videoTransport_->copyLogContextFrom(this);
    videoTransport_->start();
  } else {
    if (videoTransport_.get() == nullptr && videoEnabled_) {
      // For now we don't re/check transports, if they are already created we leave them there
      videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              listener, iceConfig_ , "", "", true, worker_, io_worker_));
      videoTransport_->copyLogContextFrom(this);
      videoTransport_->start();
    }
    if (audioTransport_.get() == nullptr && audioEnabled_) {
      audioTransport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, true,
                                              listener, iceConfig_, "", "", true, worker_, io_worker_));
      audioTransport_->copyLogContextFrom(this);
      audioTransport_->start();
    }
  }
  if (connEventListener_ != nullptr) {
    std::string msg = this->getLocalSdp();
    connEventListener_->notifyEvent(globalState_, msg);
  }
  media_stream_->setLocalSdp(local_sdp_);
  return true;
}

void WebRtcConnection::addMediaStream(std::shared_ptr<MediaStream> media_stream) {
  ELOG_DEBUG("%s message: Adding mediaStream, id: %s", toLog(), media_stream->getId().c_str());
  media_stream_ = media_stream;
}

void WebRtcConnection::removeMediaStream(const std::string& stream_id) {
  ELOG_DEBUG("%s message: removing mediaStream, id: %s", toLog(), stream_id.c_str())
  media_stream_.reset();
}

bool WebRtcConnection::setRemoteSdp(const std::string &sdp) {
  ELOG_DEBUG("%s message: setting remote SDP", toLog());

  if (!sending_) {
    return true;
  }

  remote_sdp_->initWithSdp(sdp, "");

  bundle_ = remote_sdp_->isBundle;
  local_sdp_->setOfferSdp(remote_sdp_);
  extProcessor_.setSdpInfo(local_sdp_);
  local_sdp_->updateSupportedExtensionMap(extProcessor_.getSupportedExtensionMap());

  local_sdp_->video_ssrc_list.push_back(media_stream_->getVideoSinkSSRC());
  local_sdp_->audio_ssrc = media_stream_->getAudioSinkSSRC();

  if (remote_sdp_->dtlsRole == ACTPASS) {
    local_sdp_->dtlsRole = ACTIVE;
  }

  audioEnabled_ = remote_sdp_->hasAudio;
  videoEnabled_ = remote_sdp_->hasVideo;

  if (remote_sdp_->profile == SAVPF) {
    if (remote_sdp_->isFingerprint) {
      auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());
      if (remote_sdp_->hasVideo || bundle_) {
        std::string username = remote_sdp_->getUsername(VIDEO_TYPE);
        std::string password = remote_sdp_->getPassword(VIDEO_TYPE);
        if (videoTransport_.get() == nullptr) {
          ELOG_DEBUG("%s message: Creating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, remote_sdp_->isRtcpMux,
                                                  listener, iceConfig_ , username, password, false,
                                                  worker_, io_worker_));
          videoTransport_->copyLogContextFrom(this);
          videoTransport_->start();
        } else {
          ELOG_DEBUG("%s message: Updating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          videoTransport_->getIceConnection()->setRemoteCredentials(username, password);
        }
      }
      if (!bundle_ && remote_sdp_->hasAudio) {
        std::string username = remote_sdp_->getUsername(AUDIO_TYPE);
        std::string password = remote_sdp_->getPassword(AUDIO_TYPE);
        if (audioTransport_.get() == nullptr) {
          ELOG_DEBUG("%s message: Creating audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audioTransport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, remote_sdp_->isRtcpMux,
                                                  listener, iceConfig_, username, password, false,
                                                  worker_, io_worker_));
          audioTransport_->copyLogContextFrom(this);
          audioTransport_->start();
        } else {
          ELOG_DEBUG("%s message: Update audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audioTransport_->getIceConnection()->setRemoteCredentials(username, password);
        }
      }
    }
  }
  if (this->getCurrentState() >= CONN_GATHERED) {
    if (!remote_sdp_->getCandidateInfos().empty()) {
      ELOG_DEBUG("%s message: Setting remote candidates after gathered", toLog());
      if (remote_sdp_->hasVideo) {
        videoTransport_->setRemoteCandidates(remote_sdp_->getCandidateInfos(), bundle_);
      }
      if (!bundle_ && remote_sdp_->hasAudio) {
        audioTransport_->setRemoteCandidates(remote_sdp_->getCandidateInfos(), bundle_);
      }
    }
  }

  if (trickleEnabled_) {
    std::string object = this->getLocalSdp();
    if (connEventListener_) {
      connEventListener_->notifyEvent(CONN_SDP, object);
    }
  }

  media_stream_->setRemoteSdp(remote_sdp_);
  media_stream_->setLocalSdp(local_sdp_);
  return true;
}


bool WebRtcConnection::addRemoteCandidate(const std::string &mid, int mLineIndex, const std::string &sdp) {
  // TODO(pedro) Check type of transport.
  ELOG_DEBUG("%s message: Adding remote Candidate, candidate: %s, mid: %s, sdpMLine: %d",
              toLog(), sdp.c_str(), mid.c_str(), mLineIndex);
  if (videoTransport_ == nullptr && audioTransport_ == nullptr) {
    ELOG_WARN("%s message: addRemoteCandidate on NULL transport", toLog());
    return false;
  }
  MediaType theType;
  std::string theMid;

  // TODO(pedro) check if this works with video+audio and no bundle
  if (mLineIndex == -1) {
    ELOG_DEBUG("%s message: All candidates received", toLog());
    if (videoTransport_) {
      videoTransport_->getIceConnection()->setReceivedLastCandidate(true);
    } else if (audioTransport_) {
      audioTransport_->getIceConnection()->setReceivedLastCandidate(true);
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
      res = videoTransport_->setRemoteCandidates(tempSdp.getCandidateInfos(), bundle_);
    } else if (theType == AUDIO_TYPE) {
      res = audioTransport_->setRemoteCandidates(tempSdp.getCandidateInfos(), bundle_);
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
  if (videoTransport_ != nullptr && getCurrentState() != CONN_READY) {
    videoTransport_->processLocalSdp(local_sdp_.get());
  }
  if (!bundle_ && audioTransport_ != nullptr && getCurrentState() != CONN_READY) {
    audioTransport_->processLocalSdp(local_sdp_.get());
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
  if (trickleEnabled_) {
    if (connEventListener_ != nullptr) {
      if (!bundle_) {
        std::string object = this->getJSONCandidate(transport->transport_name, sdp);
        connEventListener_->notifyEvent(CONN_CANDIDATE, object);
      } else {
        if (remote_sdp_->hasAudio) {
          std::string object = this->getJSONCandidate("audio", sdp);
          connEventListener_->notifyEvent(CONN_CANDIDATE, object);
        }
        if (remote_sdp_->hasVideo) {
          std::string object2 = this->getJSONCandidate("video", sdp);
          connEventListener_->notifyEvent(CONN_CANDIDATE, object2);
        }
      }
    }
  }
}

void WebRtcConnection::onTransportData(std::shared_ptr<DataPacket> packet, Transport *transport) {
  if (getCurrentState() != CONN_READY) {
    return;
  }
  media_stream_->onTransportData(packet, transport);
}

int WebRtcConnection::sendPLI() {
  return media_stream_->sendPLI();
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
  boost::mutex::scoped_lock lock(updateStateMutex_);
  WebRTCEvent temp = globalState_;
  std::string msg = "";
  ELOG_DEBUG("%s transportName: %s, new_state: %d", toLog(), transport->transport_name.c_str(), state);
  if (videoTransport_.get() == nullptr && audioTransport_.get() == nullptr) {
    ELOG_ERROR("%s message: Updating NULL transport, state: %d", toLog(), state);
    return;
  }
  if (globalState_ == CONN_FAILED) {
    // if current state is failed -> noop
    return;
  }
  switch (state) {
    case TRANSPORT_STARTED:
      if (bundle_) {
        temp = CONN_STARTED;
      } else {
        if ((!remote_sdp_->hasAudio || (audioTransport_.get() != nullptr
                  && audioTransport_->getTransportState() == TRANSPORT_STARTED)) &&
            (!remote_sdp_->hasVideo || (videoTransport_.get() != nullptr
                  && videoTransport_->getTransportState() == TRANSPORT_STARTED))) {
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
            videoTransport_->setRemoteCandidates(remote_sdp_->getCandidateInfos(), bundle_);
          }
          if (!bundle_ && remote_sdp_->hasAudio) {
            audioTransport_->setRemoteCandidates(remote_sdp_->getCandidateInfos(), bundle_);
          }
        }
        if (!trickleEnabled_) {
          temp = CONN_GATHERED;
          msg = this->getLocalSdp();
        }
      } else {
        if ((!local_sdp_->hasAudio || (audioTransport_.get() != nullptr
                  && audioTransport_->getTransportState() == TRANSPORT_GATHERED)) &&
            (!local_sdp_->hasVideo || (videoTransport_.get() != nullptr
                  && videoTransport_->getTransportState() == TRANSPORT_GATHERED))) {
            // WebRTCConnection will be ready only when all channels are ready.
            if (!trickleEnabled_) {
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
        if (media_stream_) {
          media_stream_->sendPLIToFeedback();
        }
      } else {
        if ((!remote_sdp_->hasAudio || (audioTransport_.get() != nullptr
                  && audioTransport_->getTransportState() == TRANSPORT_READY)) &&
            (!remote_sdp_->hasVideo || (videoTransport_.get() != nullptr
                  && videoTransport_->getTransportState() == TRANSPORT_READY))) {
            // WebRTCConnection will be ready only when all channels are ready.
            temp = CONN_READY;
            trackTransportInfo();
            if (media_stream_) {
              media_stream_->sendPLIToFeedback();
            }
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

  if (audioTransport_.get() != nullptr && videoTransport_.get() != nullptr) {
    ELOG_DEBUG("%s message: %s, transportName: %s, videoTransportState: %d"
              ", audioTransportState: %d, calculatedState: %d, globalState: %d",
      toLog(),
      "Update Transport State",
      transport->transport_name.c_str(),
      static_cast<int>(audioTransport_->getTransportState()),
      static_cast<int>(videoTransport_->getTransportState()),
      static_cast<int>(temp),
      static_cast<int>(globalState_));
  }

  if (globalState_ == temp) {
    return;
  }

  globalState_ = temp;

  if (connEventListener_ != nullptr) {
    ELOG_INFO("%s newGlobalState: %d", toLog(), globalState_);
    connEventListener_->notifyEvent(globalState_, msg);
  }
}

void WebRtcConnection::trackTransportInfo() {
  CandidatePair candidate_pair;
  if (videoEnabled_ && videoTransport_) {
    candidate_pair = videoTransport_->getIceConnection()->getSelectedPair();
    asyncTask([candidate_pair] (std::shared_ptr<WebRtcConnection> connection) {
      std::shared_ptr<Stats> stats = connection->stats_;
      uint32_t video_sink_ssrc = connection->getMediaStream()->getVideoSinkSSRC();
      uint32_t video_source_ssrc = connection->getMediaStream()->getVideoSourceSSRC();

      if (video_sink_ssrc != kDefaultVideoSinkSSRC) {
        stats->getNode()[video_sink_ssrc].insertStat("clientHostType",
                                                     StringStat{candidate_pair.clientHostType});
      }
      if (video_source_ssrc != 0) {
        stats->getNode()[video_source_ssrc].insertStat("clientHostType",
                                                       StringStat{candidate_pair.clientHostType});
      }
    });
  }

  if (audioEnabled_) {
    if (audioTransport_) {
      candidate_pair = audioTransport_->getIceConnection()->getSelectedPair();
    }
    asyncTask([candidate_pair] (std::shared_ptr<WebRtcConnection> connection) {
      std::shared_ptr<Stats> stats = connection->stats_;
      uint32_t audio_sink_ssrc = connection->getMediaStream()->getAudioSinkSSRC();
      uint32_t audio_source_ssrc = connection->getMediaStream()->getAudioSourceSSRC();

      if (audio_sink_ssrc != kDefaultAudioSinkSSRC) {
        stats->getNode()[audio_sink_ssrc].insertStat("clientHostType",
                                                     StringStat{candidate_pair.clientHostType});
      }
      if (audio_source_ssrc != 0) {
        stats->getNode()[audio_source_ssrc].insertStat("clientHostType",
                                                       StringStat{candidate_pair.clientHostType});
      }
    });
  }
}

// changes the outgoing payload type for in the given data packet

void WebRtcConnection::setSlideShowMode(bool state) {
  media_stream_->setSlideShowMode(state);
}

void WebRtcConnection::muteStream(bool mute_video, bool mute_audio) {
  media_stream_->muteStream(mute_video, mute_audio);
}

void WebRtcConnection::setVideoConstraints(int max_video_width, int max_video_height, int max_video_frame_rate) {
  media_stream_->setVideoConstraints(max_video_width, max_video_height,
  max_video_frame_rate);
}

void WebRtcConnection::setFeedbackReports(bool will_send_fb, uint32_t target_bitrate) {
  media_stream_->setFeedbackReports(will_send_fb, target_bitrate);
}

void WebRtcConnection::setMetadata(std::map<std::string, std::string> metadata) {
  setLogContext(metadata);
}

WebRTCEvent WebRtcConnection::getCurrentState() {
  return globalState_;
}

void WebRtcConnection::getJSONStats(std::function<void(std::string)> callback) {
  media_stream_->getJSONStats(callback);
}

void WebRtcConnection::write(std::shared_ptr<DataPacket> packet) {
  Transport *transport = (bundle_ || packet->type == VIDEO_PACKET) ? videoTransport_.get() : audioTransport_.get();
  if (transport == nullptr) {
    return;
  }
  this->extProcessor_.processRtpExtensions(packet);
  transport->write(packet->data, packet->length);
}

void WebRtcConnection::setQualityLayer(int spatial_layer, int temporal_layer) {
  media_stream_->setQualityLayer(spatial_layer, temporal_layer);
}

}  // namespace erizo
