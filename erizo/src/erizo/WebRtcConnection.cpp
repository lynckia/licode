/*
 * WebRTCConnection.cpp
 */

#include <cstdio>
#include <map>
#include <queue>
#include <algorithm>
#include <string>
#include <cstring>

#include "WebRtcConnection.h"
#include "DtlsTransport.h"
#include "SdpInfo.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtcpAggregator.h"
#include "rtp/RtcpForwarder.h"
#include "rtp/RtpRetransmissionHandler.h"

namespace erizo {
DEFINE_LOGGER(WebRtcConnection, "WebRtcConnection");

WebRtcConnection::WebRtcConnection(const std::string& connection_id, bool audioEnabled, bool videoEnabled,
    const IceConfig& iceConfig, WebRtcConnectionEventListener* listener)
    : connection_id_(connection_id), audioEnabled_(audioEnabled), videoEnabled_(videoEnabled),
      connEventListener_(listener), iceConfig_(iceConfig), fec_receiver_(this),
      pipeline_{Pipeline::create()} {
  ELOG_INFO("%s, message: constructor, stunserver: %s, stunPort: %d, minPort: %d, maxPort: %d",
      toLog(), iceConfig.stunServer.c_str(), iceConfig.stunPort, iceConfig.minPort, iceConfig.maxPort);
  bundle_ = false;
  setVideoSinkSSRC(55543);
  setAudioSinkSSRC(44444);
  videoSink_ = NULL;
  audioSink_ = NULL;
  fbSink_ = NULL;
  sourcefbSink_ = this;
  sinkfbSource_ = this;
  globalState_ = CONN_INITIAL;

  pipeline_->addBack(PacketWriter(this));
  pipeline_->addBack(RtpRetransmissionHandler(this));
  pipeline_->addBack(PacketReader(this));
  pipeline_->finalize();

  trickleEnabled_ = iceConfig_.shouldTrickle;

  shouldSendFeedback_ = true;
  slideShowMode_ = false;

  gettimeofday(&mark_, NULL);

  rateControl_ = 0;
  seqNo_ = 1000;
  sendSeqNo_ = 0;
  grace_ = 0;
  seqNoOffset_ = 0;
  sending_ = true;
}

WebRtcConnection::~WebRtcConnection() {
  ELOG_DEBUG("%s, message:Destructor called", toLog());
  sending_ = false;
  cond_.notify_one();
  send_Thread_.join();
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
  ELOG_DEBUG("%s, message: Destructor ended", toLog());
}

bool WebRtcConnection::init() {
  rtcpProcessor_ = boost::shared_ptr<RtcpProcessor>(
                    new RtcpForwarder(static_cast<MediaSink*>(this), static_cast<MediaSource*>(this)));
  send_Thread_ = boost::thread(&WebRtcConnection::sendLoop, this);
  if (connEventListener_ != NULL) {
    connEventListener_->notifyEvent(globalState_, "");
  }
  return true;
}

// TODO(pedro): Erizo Should accept hints to create the Offer
bool WebRtcConnection::createOffer() {
  bundle_ = true;
  videoEnabled_ = true;
  audioEnabled_ = true;
  this->localSdp_.createOfferSdp(videoEnabled_, audioEnabled_);
  ELOG_DEBUG("%s, message: Creating sdp offer, isBundle: %d", toLog(), bundle_);
  if (videoEnabled_)
    localSdp_.videoSsrc = this->getVideoSinkSSRC();
  if (audioEnabled_)
    localSdp_.audioSsrc = this->getAudioSinkSSRC();

  if (bundle_) {
    videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                            this, iceConfig_ , "", "", true));
    videoTransport_->start();
  } else {
    if (videoTransport_.get() == NULL && videoEnabled_) {
      // For now we don't re/check transports, if they are already created we leave them there
      videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, true,
                                              this, iceConfig_ , "", "", true));
      videoTransport_->start();
    }
    if (audioTransport_.get() == NULL && audioEnabled_) {
      audioTransport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, true,
                                              this, iceConfig_, "", "", true));
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
  ELOG_DEBUG("%s, message: setting remote SDP", toLog());
  remoteSdp_.initWithSdp(sdp, "");

  bundle_ = remoteSdp_.isBundle;
  localSdp_.setOfferSdp(remoteSdp_);
  extProcessor_.setSdpInfo(localSdp_);

  localSdp_.videoSsrc = this->getVideoSinkSSRC();
  localSdp_.audioSsrc = this->getAudioSinkSSRC();

  this->setVideoSourceSSRC(remoteSdp_.videoSsrc);
  this->thisStats_.setVideoSourceSSRC(this->getVideoSourceSSRC());
  this->setAudioSourceSSRC(remoteSdp_.audioSsrc);
  this->thisStats_.setAudioSourceSSRC(this->getAudioSourceSSRC());
  this->audioEnabled_ = remoteSdp_.hasAudio;
  this->videoEnabled_ = remoteSdp_.hasVideo;
  rtcpProcessor_->addSourceSsrc(this->getAudioSourceSSRC());
  rtcpProcessor_->addSourceSsrc(this->getVideoSourceSSRC());

  if (remoteSdp_.profile == SAVPF) {
    if (remoteSdp_.isFingerprint) {
      if (remoteSdp_.hasVideo || bundle_) {
        std::string username = remoteSdp_.getUsername(VIDEO_TYPE);
        std::string password = remoteSdp_.getPassword(VIDEO_TYPE);
        if (videoTransport_.get() == NULL) {
          ELOG_DEBUG("%s, message: Creating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          videoTransport_.reset(new DtlsTransport(VIDEO_TYPE, "video", connection_id_, bundle_, remoteSdp_.isRtcpMux,
                                                  this, iceConfig_ , username, password, false));
          videoTransport_->start();
        } else {
          ELOG_DEBUG("%s, message: Updating videoTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          videoTransport_->getNiceConnection()->setRemoteCredentials(username, password);
        }
      }
      if (!bundle_ && remoteSdp_.hasAudio) {
        std::string username = remoteSdp_.getUsername(AUDIO_TYPE);
        std::string password = remoteSdp_.getPassword(AUDIO_TYPE);
        if (audioTransport_.get() == NULL) {
          ELOG_DEBUG("%s, message: Creating audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audioTransport_.reset(new DtlsTransport(AUDIO_TYPE, "audio", connection_id_, bundle_, remoteSdp_.isRtcpMux,
                                                  this, iceConfig_, username, password, false));
          audioTransport_->start();
        } else {
          ELOG_DEBUG("%s, message: Update audioTransport, ufrag: %s, pass: %s",
                      toLog(), username.c_str(), password.c_str());
          audioTransport_->getNiceConnection()->setRemoteCredentials(username, password);
        }
      }
    }
  }
  if (this->getCurrentState() >= CONN_GATHERED) {
    if (!remoteSdp_.getCandidateInfos().empty()) {
      ELOG_DEBUG("%s, message: Setting remote candidates after gathered", toLog());
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
    ELOG_DEBUG("%s, message: Setting remote BW, maxVideoBW: %u", toLog(), remoteSdp_.videoBandwidth);
    this->rtcpProcessor_->setMaxVideoBW(remoteSdp_.videoBandwidth*1000);
  }

  return true;
}

bool WebRtcConnection::addRemoteCandidate(const std::string &mid, int mLineIndex, const std::string &sdp) {
  // TODO(pedro) Check type of transport.
  ELOG_DEBUG("%s, message: Adding remote Candidate, candidate: %s, mid: %s, sdpMLine: %d",
              toLog(), sdp.c_str(), mid.c_str(), mLineIndex);
  if (videoTransport_ == NULL) {
    ELOG_WARN("%s, message: addRemoteCandidate on NULL transport", toLog());
    return false;
  }
  MediaType theType;
  std::string theMid;
  // Checking if it's the last candidate, only works in bundle.
  if (mLineIndex == -1) {
    ELOG_DEBUG("%s, message: All candidates received", toLog());
    videoTransport_->getNiceConnection()->setReceivedLastCandidate(true);
  }
  if ((!mid.compare("video")) || (mLineIndex == remoteSdp_.videoSdpMLine)) {
    theType = VIDEO_TYPE;
    theMid = "video";
  } else {
    theType = AUDIO_TYPE;
    theMid = "audio";
  }
  SdpInfo tempSdp;
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
      ELOG_ERROR("%s, message: add remote candidate with no Media (video or audio), candidate: %s",
                  toLog(), sdp.c_str() );
    }
  }

  for (uint8_t it = 0; it < tempSdp.getCandidateInfos().size(); it++) {
    remoteSdp_.addCandidate(tempSdp.getCandidateInfos()[it]);
  }
  return res;
}

std::string WebRtcConnection::getLocalSdp() {
  ELOG_DEBUG("%s, message: Getting Local Sdp", toLog());
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
  ELOG_DEBUG("%s, message: Discovered New Candidate, candidate: %s", toLog(), sdp.c_str());
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
        queueData(std::make_shared<dataPacket>(0, buf, len, AUDIO_PACKET, 0));
      }
    }
  } else if (audioTransport_.get() != NULL) {
    if (audioEnabled_ == true) {
        queueData(std::make_shared<dataPacket>(0, buf, len, AUDIO_PACKET, 0));
    }
  }
  return len;
}

// This is called by our fec_ object when it recovers a packet.
bool WebRtcConnection::OnRecoveredPacket(const uint8_t* rtp_packet, int rtp_packet_length) {
  this->deliverVideoData_((char*)rtp_packet, rtp_packet_length);  // NOLINT
  return true;
}

int32_t WebRtcConnection::OnReceivedPayloadData(const uint8_t* /*payload_data*/, const uint16_t /*payload_size*/,
                                                const webrtc::WebRtcRTPHeader* /*rtp_header*/) {
    // Unused by WebRTC's FEC implementation; just something we have to implement.
    return 0;
}

int WebRtcConnection::deliverVideoData_(char* buf, int len) {
  if (videoTransport_.get() != NULL) {
    if (videoEnabled_ == true) {
      RtcpHeader* hc = reinterpret_cast<RtcpHeader*>(buf);
      if (hc->isRtcp()) {
        queueData(std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET, 0));
        return len;
      }
      RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
      sendSeqNo_ = h->getSeqNumber();
      if (h->getPayloadType() == RED_90000_PT && (!remoteSdp_.supportPayloadType(RED_90000_PT) || slideShowMode_)) {
        // This is a RED/FEC payload, but our remote endpoint doesn't support that
        // (most likely because it's firefox :/ )
        // Let's go ahead and run this through our fec receiver to convert it to raw VP8
        webrtc::RTPHeader hackyHeader;
        hackyHeader.headerLength = h->getHeaderLength();
        hackyHeader.sequenceNumber = h->getSeqNumber();
        // FEC copies memory, manages its own memory, including memory passed in callbacks (in the callback,
        // be sure to memcpy out of webrtc's buffers
        if (fec_receiver_.AddReceivedRedPacket(hackyHeader, (const uint8_t*) buf, len, ULP_90000_PT) == 0) {
          fec_receiver_.ProcessReceivedFec();
        }
      } else {
        if (slideShowMode_) {
          RtpVP8Parser parser;
          RTPPayloadVP8* payload = parser.parseVP8(
                          reinterpret_cast<unsigned char*>(buf + h->getHeaderLength()), len - h->getHeaderLength());
          if (!payload->frameType) {  // Its a keyframe
            grace_ = 1;
          }
          delete payload;
          if (grace_) {  // We send until marker
            queueData(std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET, seqNo_++));
            if (h->getMarker()) {
              grace_ = 0;
            }
          }
        } else {
          int16_t seq_num = seqNoOffset_ > 0 ? (sendSeqNo_ - seqNoOffset_) : 0;
          queueData(std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET, seq_num));
        }
      }
    }
  }
  return len;
}

int WebRtcConnection::deliverFeedback_(char* buf, int len) {
  int newLength = rtcpProcessor_->analyzeFeedback(buf, len);
  if (newLength) {
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(buf);
    uint32_t recvSSRC = chead->getSourceSSRC();
    if (recvSSRC == this->getVideoSourceSSRC()) {
      queueData(std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET, 0));
    } else if (recvSSRC == this->getAudioSourceSSRC()) {
      queueData(std::make_shared<dataPacket>(0, buf, len, AUDIO_PACKET, 0));
    } else {
      ELOG_DEBUG("%s, unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                  toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
    }
    return newLength;
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

  pipeline_->read(packet);
}

void WebRtcConnection::read(std::shared_ptr<dataPacket> packet) {
  char* buf = packet->data;
  int len = packet->length;
  // PROCESS RTCP
  RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
  uint32_t recvSSRC;
  if (chead->isRtcp()) {
    thisStats_.processRtcpPacket(buf, len);
    if (chead->packettype == RTCP_Sender_PT) {  // Sender Report
      rtcpProcessor_->analyzeSr(chead);
      recvSSRC = chead->getSSRC();
    }
  } else {
    uint32_t bitRate = thisStats_.processRtpPacket(buf, len);  // Take into account ALL RTP traffic
    if (bitRate) {
      this->rtcpProcessor_->setPublisherBW(bitRate);
    }
    recvSSRC = head->getSSRC();
  }

  // DELIVER FEEDBACK (RR, FEEDBACK PACKETS)
  if (chead->isFeedback()) {
    if (fbSink_ != NULL && shouldSendFeedback_) {
      // we want to send feedback, check if we need to alter packets
      if (seqNoOffset_ > 0) {
        char* movingBuf = buf;
        int rtcpLength = 0;
        int totalLength = 0;
        do {
          movingBuf += rtcpLength;
          chead = reinterpret_cast<RtcpHeader*>(movingBuf);
          rtcpLength = (ntohs(chead->length) + 1) * 4;
          totalLength += rtcpLength;
          switch (chead->packettype) {
            case RTCP_Receiver_PT:
              if ((chead->getHighestSeqnum() + seqNoOffset_) < chead->getHighestSeqnum()) {
                // The seqNo adjustment causes a wraparound, add to cycles
                chead->setSeqnumCycles(chead->getSeqnumCycles() + 1);
              }
              chead->setHighestSeqnum(chead->getHighestSeqnum() + seqNoOffset_);

              break;
            case RTCP_RTP_Feedback_PT:
              chead->setNackPid(chead->getNackPid() + seqNoOffset_);
              break;
            default:
              break;
          }
        } while (totalLength < len);
      }
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
        ELOG_DEBUG("%s, unknownSSRC: %u, localVideoSSRC: %u, localAudioSSRC: %u",
                    toLog(), recvSSRC, this->getVideoSourceSSRC(), this->getAudioSourceSSRC());
      }
    } else {
      if (packet->type == AUDIO_PACKET && audioSink_ != NULL) {
        parseIncomingPayloadType(buf, len, AUDIO_PACKET);
        // Firefox does not send SSRC in SDP
        if (this->getAudioSourceSSRC() == 0) {
          ELOG_DEBUG("%s, discoveredAudioSourceSSRC:%u", toLog(), recvSSRC);
          this->setAudioSourceSSRC(recvSSRC);
        }
        audioSink_->deliverAudioData(buf, len);
      } else if (packet->type == VIDEO_PACKET && videoSink_ != NULL) {
        parseIncomingPayloadType(buf, len, VIDEO_PACKET);
        // Firefox does not send SSRC in SDP
        if (this->getVideoSourceSSRC() == 0) {
          ELOG_DEBUG("%s, discoveredVideoSourceSSRC:%u", toLog(), recvSSRC);
          this->setVideoSourceSSRC(recvSSRC);
        }
        // change ssrc for RTP packets, don't touch here if RTCP
        videoSink_->deliverVideoData(buf, len);
      }
    }  // if not bundle
  }  // if not Feedback

  // check if we need to send FB || RR messages
  rtcpProcessor_->checkRtcpFb();
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
  queueData(std::make_shared<dataPacket>(0, buf, len, VIDEO_PACKET, 0));
  return len;
}

void WebRtcConnection::updateState(TransportState state, Transport * transport) {
  boost::mutex::scoped_lock lock(updateStateMutex_);
  WebRTCEvent temp = globalState_;
  std::string msg = "";
  ELOG_DEBUG("%s, transportName: %s, new_state: %d", toLog(), transport->transport_name.c_str(), state);
  if (videoTransport_.get() == NULL && audioTransport_.get() == NULL) {
    ELOG_ERROR("%s, message: Updating NULL transport, state: %d", toLog(), state);
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
      ELOG_ERROR("%s, message: Transport Failed, transportType: %s", toLog(), transport->transport_name.c_str() );
      cond_.notify_one();
      break;
    default:
      ELOG_DEBUG("%s, message: Doing nothing on state, state %d", toLog(), state);
      break;
  }

  if (audioTransport_.get() != NULL && videoTransport_.get() != NULL) {
    ELOG_DEBUG("%s, message: %s, transportName: %s, videoTransportState: %d"
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
    ELOG_INFO("%s, newGlobalState: %d", toLog(), globalState_);
    connEventListener_->notifyEvent(globalState_, msg);
  }
}

// changes the outgoing payload type for in the given data packet
void WebRtcConnection::queueData(std::shared_ptr<dataPacket> packet) {
  // if ((audioSink_ == NULL && videoSink_ == NULL && fbSink_ == NULL) || !sending_)
    // we don't enqueue data if there is nothing to receive it
    // return;
  boost::mutex::scoped_lock lock(receiveVideoMutex_);
  if (!sending_) {
    return;
  }
  if (packet->comp == -1) {
    sending_ = false;
    std::queue<std::shared_ptr<dataPacket>> empty;
    std::swap(sendQueue_, empty);
    auto p_ = std::make_shared<dataPacket>();
    p_->comp = -1;
    sendQueue_.push(p_);
    cond_.notify_one();
    return;
  }
  if (sendQueue_.size() < 1000) {
    changeDeliverPayloadType(packet.get(), packet->type);
    if (packet->seq_num) {
      RtpHeader* h = reinterpret_cast<RtpHeader*>(&packet->data);
      h->setSeqNumber(packet->seq_num);
    }
    sendQueue_.push(packet);
  } else {
    ELOG_WARN("%s, message: Queue Discarding packets", toLog());
  }
  cond_.notify_one();
}

void WebRtcConnection::setSlideShowMode(bool state) {
  ELOG_DEBUG("%s, slideShowMode: %u", toLog(), state);
  if (slideShowMode_ == state) {
    return;
  }
  if (state == true) {
    seqNo_ = sendSeqNo_ - seqNoOffset_;
    grace_ = 0;
    slideShowMode_ = true;
    shouldSendFeedback_ = false;
    ELOG_DEBUG("%s, message: Setting seqNo, seqNo: %u", toLog(), seqNo_);
  } else {
    seqNoOffset_ = sendSeqNo_ - seqNo_ + 1;
    ELOG_DEBUG("%s, message: Changing offset manually, sendSeqNo: %u, seqNo: %u, offset: %u",
                toLog(), sendSeqNo_, seqNo_, seqNoOffset_);
    slideShowMode_ = false;
    shouldSendFeedback_ = true;
  }
}

WebRTCEvent WebRtcConnection::getCurrentState() {
  return globalState_;
}

std::string WebRtcConnection::getJSONStats() {
  return thisStats_.getStats();
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

void WebRtcConnection::sendLoop() {
  uint32_t partial_bitrate = 0;
  uint64_t sentVideoBytes = 0;
  uint64_t lastSecondVideoBytes = 0;
    while (sending_) {
        std::shared_ptr<dataPacket> p;
        {
            boost::unique_lock<boost::mutex> lock(receiveVideoMutex_);
            while (sendQueue_.size() == 0) {
                cond_.wait(lock);
                if (!sending_) {
                    return;
                }
            }
            if (sendQueue_.front()->comp ==-1) {
                sending_ =  false;
                ELOG_DEBUG("%s, message: finishing send Thread", toLog());
                sendQueue_.pop();
                return;
            }

            p = sendQueue_.front();
            sendQueue_.pop();
        }
        if (rateControl_ && !slideShowMode_) {
          if (p->type == VIDEO_PACKET) {
            if (rateControl_ == 1) {
              continue;
            }
            gettimeofday(&now_, NULL);
            uint64_t nowms = (now_.tv_sec * 1000) + (now_.tv_usec / 1000);
            uint64_t markms = (mark_.tv_sec * 1000) + (mark_.tv_usec/1000);
            if ((nowms - markms) >= 100) {
              mark_ = now_;
              lastSecondVideoBytes = sentVideoBytes;
            }
            partial_bitrate = ((sentVideoBytes - lastSecondVideoBytes) * 8) * 10;
            if (partial_bitrate > this->rateControl_) {
              continue;
            }
            sentVideoBytes += p->length;
          }
        }
        pipeline_->write(p);
    }
}

}  // namespace erizo
