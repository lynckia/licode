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
#include "DtlsTransport.h"
#include "SdpInfo.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtcpAggregator.h"
#include "rtp/RtcpForwarder.h"
#include "rtp/RtpRetransmissionHandler.h"
#include "rtp/StatsHandler.h"

namespace erizo {
DEFINE_LOGGER(WebRtcConnection, "WebRtcConnection");

WebRtcConnection::WebRtcConnection(std::shared_ptr<Worker> worker, const std::string& connection_id,
    const IceConfig& iceConfig, std::vector<RtpMap> rtp_mappings, WebRtcConnectionEventListener* listener) :
    connection_id_{connection_id}, remoteSdp_{SdpInfo(rtp_mappings)}, localSdp_{SdpInfo(rtp_mappings)},
        audioEnabled_{false}, videoEnabled_{false}, bundle_{false}, connEventListener_{listener},
        iceConfig_{iceConfig}, rtp_mappings_{rtp_mappings},
        pipeline_{Pipeline::create()}, worker_{worker} {
  ELOG_INFO("%s message: constructor, stunserver: %s, stunPort: %d, minPort: %d, maxPort: %d",
      toLog(), iceConfig.stunServer.c_str(), iceConfig.stunPort, iceConfig.minPort, iceConfig.maxPort);
  setVideoSinkSSRC(55543);
  setAudioSinkSSRC(44444);
  videoSink_ = NULL;
  audioSink_ = NULL;
  fbSink_ = NULL;
  sourcefbSink_ = this;
  sinkfbSource_ = this;
  globalState_ = CONN_INITIAL;

  rtcp_processor_ = std::make_shared<RtcpForwarder>(static_cast<MediaSink*>(this), static_cast<MediaSource*>(this));

  slideshow_handler_ = std::make_shared<RtpVP8SlideShowHandler>(this);
  audio_mute_handler_ = std::make_shared<RtpAudioMuteHandler>(this);
  bwe_handler_ = std::make_shared<BandwidthEstimationHandler>(this, worker_);
  fec_handler_ = std::make_shared<FecReceiverHandler>(this);
  rtcp_processor_handler_ = std::make_shared<RtcpProcessorHandler>(this, rtcp_processor_);

  // TODO(pedro): consider creating the pipeline on setRemoteSdp or createOffer
  pipeline_->addFront(PacketReader(this));
  pipeline_->addFront(rtcp_processor_handler_);
  pipeline_->addFront(IncomingStatsHandler(this));
  pipeline_->addFront(fec_handler_);
  pipeline_->addFront(audio_mute_handler_);
  pipeline_->addFront(slideshow_handler_);
  pipeline_->addFront(bwe_handler_);
  pipeline_->addFront(RtpRetransmissionHandler(this));
  pipeline_->addFront(OutgoingStatsHandler(this));
  pipeline_->addFront(PacketWriter(this));
  pipeline_->finalize();

  // TODO(javierc): we need to improve this mechanism to enable/disable handlers using the same pipeline
  handlers_.push_back(audio_mute_handler_);
  handlers_.push_back(slideshow_handler_);
  handlers_.push_back(bwe_handler_);

  trickleEnabled_ = iceConfig_.shouldTrickle;

  shouldSendFeedback_ = true;
  slide_show_mode_ = false;

  mark_ = clock::now();

  rateControl_ = 0;
  sending_ = true;
}

WebRtcConnection::~WebRtcConnection() {
  ELOG_DEBUG("%s message:Destructor called", toLog());
  sending_ = false;
  if (videoTransport_.get()) {
    videoTransport_->close();
  }
  if (audioTransport_.get()) {
    audioTransport_->close();
  }
  globalState_ = CONN_FINISHED;
  if (connEventListener_ != NULL) {
    connEventListener_ = NULL;
  }
  videoSink_ = NULL;
  audioSink_ = NULL;
  fbSink_ = NULL;
  ELOG_DEBUG("%s message: Destructor ended", toLog());
}

void WebRtcConnection::close() {
}

bool WebRtcConnection::init() {
  if (connEventListener_ != NULL) {
    connEventListener_->notifyEvent(globalState_, "");
  }
  return true;
}

bool WebRtcConnection::createOffer(bool videoEnabled, bool audioEnabled, bool bundle) {
  bundle_ = bundle;
  videoEnabled_ = videoEnabled;
  audioEnabled_ = audioEnabled;
  this->localSdp_.createOfferSdp(videoEnabled_, audioEnabled_, bundle_);
  this->localSdp_.dtlsRole = ACTPASS;

  ELOG_DEBUG("%s message: Creating sdp offer, isBundle: %d", toLog(), bundle_);
  if (videoEnabled_)
    localSdp_.videoSsrc = this->getVideoSinkSSRC();
  if (audioEnabled_)
    localSdp_.audioSsrc = this->getAudioSinkSSRC();

  if (bundle_) {
    videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                            this, iceConfig_ , "", "", true, worker_));
    videoTransport_->copyLogContextFrom(this);
    videoTransport_->start();
  } else {
    if (videoTransport_.get() == NULL && videoEnabled_) {
      // For now we don't re/check transports, if they are already created we leave them there
      videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              this, iceConfig_ , "", "", true, worker_));
      videoTransport_->copyLogContextFrom(this);
      videoTransport_->start();
    }
    if (audioTransport_.get() == NULL && audioEnabled_) {
      audioTransport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, true,
                                              this, iceConfig_, "", "", true, worker_));
      audioTransport_->copyLogContextFrom(this);
      audioTransport_->start();
    }
  }
  if (connEventListener_ != NULL) {
    std::string msg = this->getLocalSdp();
    connEventListener_->notifyEvent(globalState_, msg);
  }
  return true;
}

bool WebRtcConnection::setRemoteSdp(const std::string &sdp) {
  ELOG_DEBUG("%s message: setting remote SDP", toLog());
  remoteSdp_.initWithSdp(sdp, "");

  bundle_ = remoteSdp_.isBundle;
  localSdp_.setOfferSdp(remoteSdp_);
  extProcessor_.setSdpInfo(localSdp_);

  bwe_handler_->updateExtensionMaps(extProcessor_.getVideoExtensionMap(), extProcessor_.getAudioExtensionMap());
  if (!remoteSdp_.supportPayloadType(RED_90000_PT) || slide_show_mode_) {
    fec_handler_->enable();
  }

  localSdp_.videoSsrc = this->getVideoSinkSSRC();
  localSdp_.audioSsrc = this->getAudioSinkSSRC();

  if (remoteSdp_.dtlsRole == ACTPASS) {
    localSdp_.dtlsRole = ACTIVE;
  }

  this->setVideoSourceSSRC(remoteSdp_.videoSsrc);
  this->stats_.setVideoSourceSSRC(this->getVideoSourceSSRC());
  this->setAudioSourceSSRC(remoteSdp_.audioSsrc);
  this->stats_.setAudioSourceSSRC(this->getAudioSourceSSRC());
  this->audioEnabled_ = remoteSdp_.hasAudio;
  this->videoEnabled_ = remoteSdp_.hasVideo;
  rtcp_processor_->addSourceSsrc(this->getAudioSourceSSRC());
  rtcp_processor_->addSourceSsrc(this->getVideoSourceSSRC());

  if (remoteSdp_.profile == SAVPF) {
    if (remoteSdp_.isFingerprint) {
      if (remoteSdp_.hasVideo || bundle_) {
        std::string username = remoteSdp_.getUsername(VIDEO_TYPE);
        std::string password = remoteSdp_.getPassword(VIDEO_TYPE);
        if (videoTransport_.get() == NULL) {
          ELOG_DEBUG("%s message: Creating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, remoteSdp_.isRtcpMux,
                                                  this, iceConfig_ , username, password, false, worker_));
          videoTransport_->copyLogContextFrom(this);
          videoTransport_->start();
        } else {
          ELOG_DEBUG("%s message: Updating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          videoTransport_->getNiceConnection()->setRemoteCredentials(username, password);
        }
      }
      if (!bundle_ && remoteSdp_.hasAudio) {
        std::string username = remoteSdp_.getUsername(AUDIO_TYPE);
        std::string password = remoteSdp_.getPassword(AUDIO_TYPE);
        if (audioTransport_.get() == NULL) {
          ELOG_DEBUG("%s message: Creating audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audioTransport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, remoteSdp_.isRtcpMux,
                                                  this, iceConfig_, username, password, false, worker_));
          audioTransport_->copyLogContextFrom(this);
          audioTransport_->start();
        } else {
          ELOG_DEBUG("%s message: Update audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audioTransport_->getNiceConnection()->setRemoteCredentials(username, password);
        }
      }
    }
  }
  if (this->getCurrentState() >= CONN_GATHERED) {
    if (!remoteSdp_.getCandidateInfos().empty()) {
      ELOG_DEBUG("%s message: Setting remote candidates after gathered", toLog());
      if (remoteSdp_.hasVideo) {
        videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos(), bundle_);
      }
      if (!bundle_ && remoteSdp_.hasAudio) {
        audioTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos(), bundle_);
      }
    }
  }

  if (trickleEnabled_) {
    std::string object = this->getLocalSdp();
    if (connEventListener_) {
      connEventListener_->notifyEvent(CONN_SDP, object);
    }
  }


  if (remoteSdp_.videoBandwidth != 0) {
    ELOG_DEBUG("%s message: Setting remote BW, maxVideoBW: %u", toLog(), remoteSdp_.videoBandwidth);
    this->rtcp_processor_->setMaxVideoBW(remoteSdp_.videoBandwidth*1000);
  }

  return true;
}

bool WebRtcConnection::addRemoteCandidate(const std::string &mid, int mLineIndex, const std::string &sdp) {
  // TODO(pedro) Check type of transport.
  ELOG_DEBUG("%s message: Adding remote Candidate, candidate: %s, mid: %s, sdpMLine: %d",
              toLog(), sdp.c_str(), mid.c_str(), mLineIndex);
  if (videoTransport_ == NULL) {
    ELOG_WARN("%s message: addRemoteCandidate on NULL transport", toLog());
    return false;
  }
  MediaType theType;
  std::string theMid;
  // Checking if it's the last candidate, only works in bundle.
  if (mLineIndex == -1) {
    ELOG_DEBUG("%s message: All candidates received", toLog());
    videoTransport_->getNiceConnection()->setReceivedLastCandidate(true);
  }
  if ((!mid.compare("video")) || (mLineIndex == remoteSdp_.videoSdpMLine)) {
    theType = VIDEO_TYPE;
    theMid = "video";
  } else {
    theType = AUDIO_TYPE;
    theMid = "audio";
  }
  SdpInfo tempSdp(rtp_mappings_);
  std::string username = remoteSdp_.getUsername(theType);
  std::string password = remoteSdp_.getPassword(theType);
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
    remoteSdp_.addCandidate(tempSdp.getCandidateInfos()[it]);
  }
  return res;
}

std::string WebRtcConnection::getLocalSdp() {
  ELOG_DEBUG("%s message: Getting Local Sdp", toLog());
  if (videoTransport_ != NULL) {
    videoTransport_->processLocalSdp(&localSdp_);
  }
  if (!bundle_ && audioTransport_ != NULL) {
    audioTransport_->processLocalSdp(&localSdp_);
  }
  localSdp_.profile = remoteSdp_.profile;
  return localSdp_.getSdp();
}

std::string WebRtcConnection::getJSONCandidate(const std::string& mid, const std::string& sdp) {
  std::map <std::string, std::string> object;
  object["sdpMid"] = mid;
  object["candidate"] = sdp;
  object["sdpMLineIndex"] = std::to_string((mid.compare("video") ? localSdp_.audioSdpMLine : localSdp_.videoSdpMLine));

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
  std::string sdp = localSdp_.addCandidate(cand);
  ELOG_DEBUG("%s message: Discovered New Candidate, candidate: %s", toLog(), sdp.c_str());
  if (trickleEnabled_) {
    if (connEventListener_ != NULL) {
      if (!bundle_) {
        std::string object = this->getJSONCandidate(transport->transport_name, sdp);
        connEventListener_->notifyEvent(CONN_CANDIDATE, object);
      } else {
        if (remoteSdp_.hasAudio) {
          std::string object = this->getJSONCandidate("audio", sdp);
          connEventListener_->notifyEvent(CONN_CANDIDATE, object);
        }
        if (remoteSdp_.hasVideo) {
          std::string object2 = this->getJSONCandidate("video", sdp);
          connEventListener_->notifyEvent(CONN_CANDIDATE, object2);
        }
      }
    }
  }
}

int WebRtcConnection::deliverAudioData_(char* buf, int len) {
  if (bundle_) {
    if (videoTransport_.get() != NULL) {
      if (audioEnabled_ == true) {
        sendPacketAsync(std::make_shared<dataPacket>(0, buf, len, AUDIO_PACKET));
      }
    }
  } else if (audioTransport_.get() != NULL) {
    if (audioEnabled_ == true) {
        sendPacketAsync(std::make_shared<dataPacket>(0, buf, len, AUDIO_PACKET));
    }
  }
  return len;
}

int WebRtcConnection::deliverVideoData_(char* buf, int len) {
  if (videoTransport_.get() != NULL) {
    if (videoEnabled_ == true) {
      sendPacketAsync(std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET));
    }
  }
  return len;
}

int WebRtcConnection::deliverFeedback_(char* buf, int len) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(buf);
  uint32_t recvSSRC = chead->getSourceSSRC();
  if (recvSSRC == this->getVideoSourceSSRC()) {
    sendPacketAsync(std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET));
  } else if (recvSSRC == this->getAudioSourceSSRC()) {
    sendPacketAsync(std::make_shared<dataPacket>(0, buf, len, AUDIO_PACKET));
  } else {
    ELOG_DEBUG("%s unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
  }
  return len;
}

void WebRtcConnection::onTransportData(std::shared_ptr<dataPacket> packet, Transport *transport) {
  if (audioSink_ == NULL && videoSink_ == NULL && fbSink_ == NULL) {
    return;
  }
  if (transport->mediaType == AUDIO_TYPE) {
    packet->type = AUDIO_PACKET;
  } else if (transport->mediaType == VIDEO_TYPE) {
    packet->type = VIDEO_PACKET;
  }

  char* buf = packet->data;
  RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
  if (!chead->isRtcp()) {
    uint32_t recvSSRC = head->getSSRC();
    if (recvSSRC == this->getVideoSourceSSRC()) {
      packet->type = VIDEO_PACKET;
    } else if (recvSSRC == this->getAudioSourceSSRC()) {
      packet->type = AUDIO_PACKET;
    }
  }

  pipeline_->read(packet);
}

void WebRtcConnection::read(std::shared_ptr<dataPacket> packet) {
  char* buf = packet->data;
  int len = packet->length;
  // PROCESS RTCP
  RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
  uint32_t recvSSRC;
  if (!chead->isRtcp()) {
    recvSSRC = head->getSSRC();
  } else if (chead->packettype == RTCP_Sender_PT) {  // Sender Report
    recvSSRC = chead->getSSRC();
  }

  // DELIVER FEEDBACK (RR, FEEDBACK PACKETS)
  if (chead->isFeedback()) {
    if (fbSink_ != NULL && shouldSendFeedback_) {
      fbSink_->deliverFeedback(buf, len);
    }
  } else {
    // RTP or RTCP Sender Report
    if (bundle_) {
      // Check incoming SSRC
      // Deliver data
      if (recvSSRC == this->getVideoSourceSSRC()) {
        parseIncomingPayloadType(buf, len, VIDEO_PACKET);
        videoSink_->deliverVideoData(buf, len);
      } else if (recvSSRC == this->getAudioSourceSSRC()) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        audioSink_->deliverAudioData(buf, len);
      } else {
        ELOG_DEBUG("%s unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                    toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
      }
    } else {
      if (packet->type == AUDIO_PACKET && audioSink_ != NULL) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        // Firefox does not send SSRC in SDP
        if (this->getAudioSourceSSRC() == 0) {
          ELOG_DEBUG("%s discoveredAudioSourceSSRC:%u", toLog(), recvSSRC);
          this->setAudioSourceSSRC(recvSSRC);
        }
        audioSink_->deliverAudioData(buf, len);
      } else if (packet->type == VIDEO_PACKET && videoSink_ != NULL) {
        parseIncomingPayloadType(buf, len, VIDEO_PACKET);
        // Firefox does not send SSRC in SDP
        if (this->getVideoSourceSSRC() == 0) {
          ELOG_DEBUG("%s discoveredVideoSourceSSRC:%u", toLog(), recvSSRC);
          this->setVideoSourceSSRC(recvSSRC);
        }
        // change ssrc for RTP packets, don't touch here if RTCP
        videoSink_->deliverVideoData(buf, len);
      }
    }  // if not bundle
  }  // if not Feedback
}

int WebRtcConnection::sendPLI() {
  RtcpHeader thePLI;
  thePLI.setPacketType(RTCP_PS_Feedback_PT);
  thePLI.setBlockCount(1);
  thePLI.setSSRC(this->getVideoSinkSSRC());
  thePLI.setSourceSSRC(this->getVideoSourceSSRC());
  thePLI.setLength(2);
  char *buf = reinterpret_cast<char*>(&thePLI);
  int len = (thePLI.getLength() + 1) * 4;
  sendPacketAsync(std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET));
  return len;
}

void WebRtcConnection::updateState(TransportState state, Transport * transport) {
  boost::mutex::scoped_lock lock(updateStateMutex_);
  WebRTCEvent temp = globalState_;
  std::string msg = "";
  ELOG_DEBUG("%s transportName: %s, new_state: %d", toLog(), transport->transport_name.c_str(), state);
  if (videoTransport_.get() == NULL && audioTransport_.get() == NULL) {
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
        if ((!remoteSdp_.hasAudio || (audioTransport_.get() != NULL
                  && audioTransport_->getTransportState() == TRANSPORT_STARTED)) &&
            (!remoteSdp_.hasVideo || (videoTransport_.get() != NULL
                  && videoTransport_->getTransportState() == TRANSPORT_STARTED))) {
            // WebRTCConnection will be ready only when all channels are ready.
            temp = CONN_STARTED;
          }
      }
      break;
    case TRANSPORT_GATHERED:
      if (bundle_) {
        if (!remoteSdp_.getCandidateInfos().empty()) {
          // Passing now new candidates that could not be passed before
          if (remoteSdp_.hasVideo) {
            videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos(), bundle_);
          }
          if (!bundle_ && remoteSdp_.hasAudio) {
            audioTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos(), bundle_);
          }
        }
        if (!trickleEnabled_) {
          temp = CONN_GATHERED;
          msg = this->getLocalSdp();
        }
      } else {
        if ((!localSdp_.hasAudio || (audioTransport_.get() != NULL
                  && audioTransport_->getTransportState() == TRANSPORT_GATHERED)) &&
            (!localSdp_.hasVideo || (videoTransport_.get() != NULL
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

      } else {
        if ((!remoteSdp_.hasAudio || (audioTransport_.get() != NULL
                  && audioTransport_->getTransportState() == TRANSPORT_READY)) &&
            (!remoteSdp_.hasVideo || (videoTransport_.get() != NULL
                  && videoTransport_->getTransportState() == TRANSPORT_READY))) {
            // WebRTCConnection will be ready only when all channels are ready.
            temp = CONN_READY;
          }
      }
      break;
    case TRANSPORT_FAILED:
      temp = CONN_FAILED;
      sending_ = false;
      msg = remoteSdp_.getSdp();
      ELOG_ERROR("%s message: Transport Failed, transportType: %s", toLog(), transport->transport_name.c_str() );
      cond_.notify_one();
      break;
    default:
      ELOG_DEBUG("%s message: Doing nothing on state, state %d", toLog(), state);
      break;
  }

  if (audioTransport_.get() != NULL && videoTransport_.get() != NULL) {
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

  if (connEventListener_ != NULL) {
    ELOG_INFO("%s newGlobalState: %d", toLog(), globalState_);
    connEventListener_->notifyEvent(globalState_, msg);
  }
}

// changes the outgoing payload type for in the given data packet
void WebRtcConnection::sendPacketAsync(std::shared_ptr<dataPacket> packet) {
  if (!sending_) {
    return;
  }
  auto conn_ptr = shared_from_this();
  if (packet->comp == -1) {
    sending_ = false;
    auto p = std::make_shared<dataPacket>();
    p->comp = -1;
    worker_->task([conn_ptr, p]{
      conn_ptr->sendPacket(p);
    });
    return;
  }

  changeDeliverPayloadType(packet.get(), packet->type);
  worker_->task([conn_ptr, packet]{
    conn_ptr->sendPacket(packet);
  });
}

void WebRtcConnection::setSlideShowMode(bool state) {
  ELOG_DEBUG("%s slideShowMode: %u", toLog(), state);
  if (slide_show_mode_ == state) {
    return;
  }
  slide_show_mode_ = state;
  slideshow_handler_->setSlideShowMode(state);
  if (!remoteSdp_.supportPayloadType(RED_90000_PT) || state) {
    fec_handler_->enable();
  } else {
    fec_handler_->disable();
  }
}

void WebRtcConnection::muteStream(bool mute_video, bool mute_audio) {
  ELOG_DEBUG("%s message: muteStream, mute_video: %u, mute_audio: %u", toLog(), mute_video, mute_audio);
  audio_mute_handler_->muteAudio(mute_audio);
}

void WebRtcConnection::setFeedbackReports(bool will_send_fb, uint32_t target_bitrate) {
  if (slide_show_mode_) {
    target_bitrate = 0;
  }

  this->shouldSendFeedback_ = will_send_fb;
  if (target_bitrate == 1) {
    this->videoEnabled_ = false;
  }
  this->rateControl_ = target_bitrate;
}

void WebRtcConnection::setMetadata(std::map<std::string, std::string> metadata) {
  setLogContext(metadata);
}

WebRTCEvent WebRtcConnection::getCurrentState() {
  return globalState_;
}

std::string WebRtcConnection::getJSONStats() {
  if (this->getVideoSourceSSRC()) {
    stats_.setEstimatedBandwidth(bwe_handler_->getLastSendBitrate(), this->getVideoSourceSSRC());
  }
  return stats_.getStats();
}

void WebRtcConnection::changeDeliverPayloadType(dataPacket *dp, packetType type) {
  RtpHeader* h = reinterpret_cast<RtpHeader*>(dp->data);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(dp->data);
  if (!chead->isRtcp()) {
      int internalPT = h->getPayloadType();
      int externalPT = internalPT;
      if (type == AUDIO_PACKET) {
          externalPT = remoteSdp_.getAudioExternalPT(internalPT);
      } else if (type == VIDEO_PACKET) {
          externalPT = remoteSdp_.getVideoExternalPT(externalPT);
      }
      if (internalPT != externalPT) {
          h->setPayloadType(externalPT);
      }
  }
}

// parses incoming payload type, replaces occurence in buf
void WebRtcConnection::parseIncomingPayloadType(char *buf, int len, packetType type) {
  RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(buf);
  RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
  if (!chead->isRtcp()) {
    int externalPT = h->getPayloadType();
    int internalPT = externalPT;
    if (type == AUDIO_PACKET) {
      internalPT = remoteSdp_.getAudioInternalPT(externalPT);
    } else if (type == VIDEO_PACKET) {
      internalPT = remoteSdp_.getVideoInternalPT(externalPT);
    }
    if (externalPT != internalPT) {
      h->setPayloadType(internalPT);
    } else {
//        ELOG_WARN("onTransportData did not find mapping for %i", externalPT);
    }
  }
}

void WebRtcConnection::write(std::shared_ptr<dataPacket> packet) {
  Transport *transport = (bundle_ || packet->type == VIDEO_PACKET) ? videoTransport_.get() :
                                                                     audioTransport_.get();
  if (transport == nullptr) {
    return;
  }
  this->extProcessor_.processRtpExtensions(packet);
  transport->write(packet->data, packet->length);
}

std::shared_ptr<Handler> WebRtcConnection::getHandler(const std::string &name) {
  std::shared_ptr<Handler> handler;
  auto it = std::find_if(handlers_.begin(), handlers_.end(), [&name](std::shared_ptr<Handler> handler) {
    return handler->getName() == name;
  });
  if (it != handlers_.end()) {
    handler = *it;
  }
  return handler;
}

void WebRtcConnection::enableHandler(const std::string &name) {
  asyncTask([name] (std::shared_ptr<WebRtcConnection> conn){
    auto handler = conn->getHandler(name);
    if (handler.get()) {
      handler->enable();
    }
  });
}

void WebRtcConnection::disableHandler(const std::string &name) {
  asyncTask([name] (std::shared_ptr<WebRtcConnection> conn){
    auto handler = conn->getHandler(name);
    if (handler.get()) {
      handler->disable();
    }
  });
}

void WebRtcConnection::asyncTask(std::function<void(std::shared_ptr<WebRtcConnection>)> f) {
  std::weak_ptr<WebRtcConnection> weak_this = shared_from_this();
  worker_->task([weak_this, f] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
  });
}

void WebRtcConnection::sendPacket(std::shared_ptr<dataPacket> p) {
  if (!sending_) {
    return;
  }
  uint32_t partial_bitrate = 0;
  uint64_t sentVideoBytes = 0;
  uint64_t lastSecondVideoBytes = 0;

  if (rateControl_ && !slide_show_mode_) {
    if (p->type == VIDEO_PACKET) {
      if (rateControl_ == 1) {
        return;
      }
      now_ = clock::now();
      if ((now_ - mark_) >= kBitrateControlPeriod) {
        mark_ = now_;
        lastSecondVideoBytes = sentVideoBytes;
      }
      partial_bitrate = ((sentVideoBytes - lastSecondVideoBytes) * 8) * 10;
      if (partial_bitrate > this->rateControl_) {
        return;
      }
      sentVideoBytes += p->length;
    }
  }
  pipeline_->write(p);
}

}  // namespace erizo
