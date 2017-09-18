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

constexpr uint32_t kDefaultVideoSinkSSRC = 55543;
constexpr uint32_t kDefaultAudioSinkSSRC = 44444;

WebRtcConnection::WebRtcConnection(std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker,
    const std::string& connection_id, const IceConfig& iceConfig, const std::vector<RtpMap> rtp_mappings,
    const std::vector<erizo::ExtMap> ext_mappings, WebRtcConnectionEventListener* listener) :
    connection_id_{connection_id}, remoteSdp_{SdpInfo(rtp_mappings)}, localSdp_{SdpInfo(rtp_mappings)},
    audioEnabled_{false}, videoEnabled_{false}, bundle_{false}, connEventListener_{listener},
    iceConfig_{iceConfig}, rtp_mappings_{rtp_mappings}, extProcessor_{ext_mappings},
    pipeline_{Pipeline::create()}, worker_{worker}, io_worker_{io_worker}, audio_muted_{false}, video_muted_{false},
    pipeline_initialized_{false} {
  ELOG_INFO("%s message: constructor, stunserver: %s, stunPort: %d, minPort: %d, maxPort: %d",
      toLog(), iceConfig.stun_server.c_str(), iceConfig.stun_port, iceConfig.min_port, iceConfig.max_port);
  setVideoSinkSSRC(kDefaultVideoSinkSSRC);
  setAudioSinkSSRC(kDefaultAudioSinkSSRC);
  source_fb_sink_ = this;
  sink_fb_source_ = this;
  stats_ = std::make_shared<Stats>();
  quality_manager_ = std::make_shared<QualityManager>();
  packet_buffer_ = std::make_shared<PacketBufferService>();
  globalState_ = CONN_INITIAL;

  rtcp_processor_ = std::make_shared<RtcpForwarder>(static_cast<MediaSink*>(this), static_cast<MediaSource*>(this));

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
    close();
  }
  ELOG_DEBUG("%s message: Destructor ended", toLog());
}

void WebRtcConnection::close() {
  ELOG_DEBUG("%s message:Close called", toLog());
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
  video_sink_ = nullptr;
  audio_sink_ = nullptr;
  fb_sink_ = nullptr;
  pipeline_->close();
  pipeline_.reset();
  ELOG_DEBUG("%s message: Close ended", toLog());
}

bool WebRtcConnection::init() {
  if (connEventListener_ != nullptr) {
    connEventListener_->notifyEvent(globalState_, "");
  }
  return true;
}

bool WebRtcConnection::isSourceSSRC(uint32_t ssrc) {
  return isVideoSourceSSRC(ssrc) || isAudioSourceSSRC(ssrc);
}

bool WebRtcConnection::isSinkSSRC(uint32_t ssrc) {
  return isVideoSinkSSRC(ssrc) || isAudioSinkSSRC(ssrc);
}


bool WebRtcConnection::createOffer(bool videoEnabled, bool audioEnabled, bool bundle) {
  bundle_ = bundle;
  videoEnabled_ = videoEnabled;
  audioEnabled_ = audioEnabled;
  this->localSdp_.createOfferSdp(videoEnabled_, audioEnabled_, bundle_);
  this->localSdp_.dtlsRole = ACTPASS;

  ELOG_DEBUG("%s message: Creating sdp offer, isBundle: %d", toLog(), bundle_);
  if (videoEnabled_)
    localSdp_.video_ssrc_list.push_back(this->getVideoSinkSSRC());
  if (audioEnabled_)
    localSdp_.audio_ssrc = this->getAudioSinkSSRC();

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
  return true;
}

bool WebRtcConnection::setRemoteSdp(const std::string &sdp) {
  ELOG_DEBUG("%s message: setting remote SDP", toLog());

  remoteSdp_.initWithSdp(sdp, "");

  if (remoteSdp_.videoBandwidth != 0) {
    ELOG_DEBUG("%s message: Setting remote BW, maxVideoBW: %u", toLog(), remoteSdp_.videoBandwidth);
    this->rtcp_processor_->setMaxVideoBW(remoteSdp_.videoBandwidth*1000);
  }

  if (pipeline_initialized_) {
    pipeline_->notifyUpdate();
    return true;
  }


  bundle_ = remoteSdp_.isBundle;
  localSdp_.setOfferSdp(remoteSdp_);
  extProcessor_.setSdpInfo(localSdp_);

  localSdp_.updateSupportedExtensionMap(extProcessor_.getSupportedExtensionMap());

  localSdp_.video_ssrc_list.push_back(getVideoSinkSSRC());
  localSdp_.audio_ssrc = getAudioSinkSSRC();

  if (remoteSdp_.dtlsRole == ACTPASS) {
    localSdp_.dtlsRole = ACTIVE;
  }
  setVideoSourceSSRCList(remoteSdp_.video_ssrc_list);
  setAudioSourceSSRC(remoteSdp_.audio_ssrc);

  audioEnabled_ = remoteSdp_.hasAudio;
  videoEnabled_ = remoteSdp_.hasVideo;

  rtcp_processor_->addSourceSsrc(getAudioSourceSSRC());
  std::for_each(video_source_ssrc_list_.begin(), video_source_ssrc_list_.end(), [this] (uint32_t new_ssrc){
      rtcp_processor_->addSourceSsrc(new_ssrc);
  });

  if (remoteSdp_.profile == SAVPF) {
    if (remoteSdp_.isFingerprint) {
      auto listener = std::dynamic_pointer_cast<TransportListener>(shared_from_this());
      if (remoteSdp_.hasVideo || bundle_) {
        std::string username = remoteSdp_.getUsername(VIDEO_TYPE);
        std::string password = remoteSdp_.getPassword(VIDEO_TYPE);
        if (videoTransport_.get() == nullptr) {
          ELOG_DEBUG("%s message: Creating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, remoteSdp_.isRtcpMux,
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
      if (!bundle_ && remoteSdp_.hasAudio) {
        std::string username = remoteSdp_.getUsername(AUDIO_TYPE);
        std::string password = remoteSdp_.getPassword(AUDIO_TYPE);
        if (audioTransport_.get() == nullptr) {
          ELOG_DEBUG("%s message: Creating audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audioTransport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, remoteSdp_.isRtcpMux,
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

  initializePipeline();

  return true;
}

void WebRtcConnection::initializePipeline() {
  pipeline_->addService(shared_from_this());
  pipeline_->addService(rtcp_processor_);
  pipeline_->addService(stats_);
  pipeline_->addService(quality_manager_);
  pipeline_->addService(packet_buffer_);

  pipeline_->addFront(PacketReader(this));

  pipeline_->addFront(LayerDetectorHandler());
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
  pipeline_->addFront(OutgoingStatsHandler());

  pipeline_->addFront(PacketWriter(this));
  pipeline_->finalize();
  pipeline_initialized_ = true;
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
  if (videoTransport_ != nullptr) {
    videoTransport_->processLocalSdp(&localSdp_);
  }
  if (!bundle_ && audioTransport_ != nullptr) {
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
    if (connEventListener_ != nullptr) {
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

int WebRtcConnection::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  if (bundle_) {
    if (videoTransport_.get() != nullptr) {
      if (audioEnabled_ == true) {
        sendPacketAsync(std::make_shared<DataPacket>(*audio_packet));
      }
    }
  } else if (audioTransport_.get() != nullptr) {
    if (audioEnabled_ == true) {
        sendPacketAsync(std::make_shared<DataPacket>(*audio_packet));
    }
  }
  return audio_packet->length;
}

int WebRtcConnection::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  if (videoTransport_.get() != nullptr) {
    if (videoEnabled_ == true) {
      sendPacketAsync(std::make_shared<DataPacket>(*video_packet));
    }
  }
  return video_packet->length;
}

int WebRtcConnection::deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(fb_packet->data);
  uint32_t recvSSRC = chead->getSourceSSRC();
  if (isVideoSourceSSRC(recvSSRC)) {
    fb_packet->type = VIDEO_PACKET;
    sendPacketAsync(fb_packet);
  } else if (isAudioSourceSSRC(recvSSRC)) {
    fb_packet->type = AUDIO_PACKET;
    sendPacketAsync(fb_packet);
  } else {
    ELOG_DEBUG("%s unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
  }
  return fb_packet->length;
}

int WebRtcConnection::deliverEvent_(MediaEventPtr event) {
  auto conn_ptr = shared_from_this();
  worker_->task([conn_ptr, event]{
    if (!conn_ptr->pipeline_initialized_) {
      return;
    }
    conn_ptr->pipeline_->notifyEvent(event);
  });
  return 1;
}

void WebRtcConnection::onTransportData(std::shared_ptr<DataPacket> packet, Transport *transport) {
  if ((audio_sink_ == nullptr && video_sink_ == nullptr && fb_sink_ == nullptr) ||
      getCurrentState() != CONN_READY) {
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
    if (isVideoSourceSSRC(recvSSRC)) {
      packet->type = VIDEO_PACKET;
    } else if (isAudioSourceSSRC(recvSSRC)) {
      packet->type = AUDIO_PACKET;
    }
  }

  if (!pipeline_initialized_) {
    ELOG_DEBUG("%s message: Pipeline not initialized yet.", toLog());
    return;
  }

  pipeline_->read(std::move(packet));
}

void WebRtcConnection::read(std::shared_ptr<DataPacket> packet) {
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
    if (fb_sink_ != nullptr && shouldSendFeedback_) {
      fb_sink_->deliverFeedback(std::move(packet));
    }
  } else {
    // RTP or RTCP Sender Report
    if (bundle_) {
      // Check incoming SSRC
      // Deliver data
      if (isVideoSourceSSRC(recvSSRC)) {
        parseIncomingPayloadType(buf, len, VIDEO_PACKET);
        video_sink_->deliverVideoData(std::move(packet));
      } else if (isAudioSourceSSRC(recvSSRC)) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        audio_sink_->deliverAudioData(std::move(packet));
      } else {
        ELOG_DEBUG("%s unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                    toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
      }
    } else {
      if (packet->type == AUDIO_PACKET && audio_sink_ != nullptr) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        // Firefox does not send SSRC in SDP
        if (getAudioSourceSSRC() == 0) {
          ELOG_DEBUG("%s discoveredAudioSourceSSRC:%u", toLog(), recvSSRC);
          this->setAudioSourceSSRC(recvSSRC);
        }
        audio_sink_->deliverAudioData(std::move(packet));
      } else if (packet->type == VIDEO_PACKET && video_sink_ != nullptr) {
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

void WebRtcConnection::notifyToEventSink(MediaEventPtr event) {
  event_sink_->deliverEvent(event);
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
  sendPacketAsync(std::make_shared<DataPacket>(0, buf, len, VIDEO_PACKET));
  return len;
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
        if ((!remoteSdp_.hasAudio || (audioTransport_.get() != nullptr
                  && audioTransport_->getTransportState() == TRANSPORT_STARTED)) &&
            (!remoteSdp_.hasVideo || (videoTransport_.get() != nullptr
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
        if ((!localSdp_.hasAudio || (audioTransport_.get() != nullptr
                  && audioTransport_->getTransportState() == TRANSPORT_GATHERED)) &&
            (!localSdp_.hasVideo || (videoTransport_.get() != nullptr
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
        if (fb_sink_) {
          fb_sink_->deliverFeedback(RtpUtils::createPLI(this->getVideoSinkSSRC(), this->getVideoSourceSSRC()));
        }
      } else {
        if ((!remoteSdp_.hasAudio || (audioTransport_.get() != nullptr
                  && audioTransport_->getTransportState() == TRANSPORT_READY)) &&
            (!remoteSdp_.hasVideo || (videoTransport_.get() != nullptr
                  && videoTransport_->getTransportState() == TRANSPORT_READY))) {
            // WebRTCConnection will be ready only when all channels are ready.
            temp = CONN_READY;
            trackTransportInfo();
            if (fb_sink_) {
              fb_sink_->deliverFeedback(RtpUtils::createPLI(this->getVideoSinkSSRC(), this->getVideoSourceSSRC()));
            }
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
      uint32_t video_sink_ssrc = connection->getVideoSinkSSRC();
      uint32_t video_source_ssrc = connection->getVideoSourceSSRC();

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
      uint32_t audio_sink_ssrc = connection->getAudioSinkSSRC();
      uint32_t audio_source_ssrc = connection->getAudioSourceSSRC();

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
void WebRtcConnection::sendPacketAsync(std::shared_ptr<DataPacket> packet) {
  if (!sending_ || getCurrentState() != CONN_READY) {
    return;
  }
  auto conn_ptr = shared_from_this();
  if (packet->comp == -1) {
    sending_ = false;
    auto p = std::make_shared<DataPacket>();
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
  asyncTask([state] (std::shared_ptr<WebRtcConnection> connection) {
    connection->stats_->getNode()[connection->getVideoSinkSSRC()].insertStat("erizoSlideShow", CumulativeStat{state});
  });
  slide_show_mode_ = state;
  notifyUpdateToHandlers();
}

void WebRtcConnection::muteStream(bool mute_video, bool mute_audio) {
  asyncTask([mute_audio, mute_video] (std::shared_ptr<WebRtcConnection> connection) {
    ELOG_DEBUG("%s message: muteStream, mute_video: %u, mute_audio: %u", connection->toLog(), mute_video, mute_audio);
    connection->audio_muted_ = mute_audio;
    connection->video_muted_ = mute_video;
    connection->stats_->getNode()[connection->getAudioSinkSSRC()].insertStat("erizoAudioMute",
                                                                             CumulativeStat{mute_audio});
    connection->stats_->getNode()[connection->getAudioSinkSSRC()].insertStat("erizoVideoMute",
                                                                             CumulativeStat{mute_video});
    connection->pipeline_->notifyUpdate();
  });
}

void WebRtcConnection::setVideoConstraints(int max_video_width, int max_video_height, int max_video_frame_rate) {
  asyncTask([max_video_width, max_video_height, max_video_frame_rate] (std::shared_ptr<WebRtcConnection> connection) {
    connection->quality_manager_->setVideoConstraints(max_video_width, max_video_height, max_video_frame_rate);
  });
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

void WebRtcConnection::getJSONStats(std::function<void(std::string)> callback) {
  asyncTask([callback] (std::shared_ptr<WebRtcConnection> connection) {
    std::string requested_stats = connection->stats_->getStats();
    //  ELOG_DEBUG("%s message: Stats, stats: %s", connection->toLog(), requested_stats.c_str());
    callback(requested_stats);
  });
}

void WebRtcConnection::changeDeliverPayloadType(DataPacket *dp, packetType type) {
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

void WebRtcConnection::write(std::shared_ptr<DataPacket> packet) {
  Transport *transport = (bundle_ || packet->type == VIDEO_PACKET) ? videoTransport_.get() :
                                                                     audioTransport_.get();
  if (transport == nullptr) {
    return;
  }
  this->extProcessor_.processRtpExtensions(packet);
  transport->write(packet->data, packet->length);
}

void WebRtcConnection::enableHandler(const std::string &name) {
  asyncTask([name] (std::shared_ptr<WebRtcConnection> conn) {
    conn->pipeline_->enable(name);
  });
}

void WebRtcConnection::disableHandler(const std::string &name) {
  asyncTask([name] (std::shared_ptr<WebRtcConnection> conn) {
    conn->pipeline_->disable(name);
  });
}

void WebRtcConnection::notifyUpdateToHandlers() {
  asyncTask([] (std::shared_ptr<WebRtcConnection> conn) {
    conn->pipeline_->notifyUpdate();
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

void WebRtcConnection::sendPacket(std::shared_ptr<DataPacket> p) {
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
  if (!pipeline_initialized_) {
    ELOG_DEBUG("%s message: Pipeline not initialized yet.", toLog());
    return;
  }

  pipeline_->write(std::move(p));
}

void WebRtcConnection::setQualityLayer(int spatial_layer, int temporal_layer) {
  asyncTask([spatial_layer, temporal_layer] (std::shared_ptr<WebRtcConnection> connection) {
    connection->quality_manager_->forceLayers(spatial_layer, temporal_layer);
  });
}

}  // namespace erizo
