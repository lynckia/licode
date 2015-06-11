/*
 * WebRTCConnection.cpp
 */

#include <cstdio>

#include "WebRtcConnection.h"
#include "DtlsTransport.h"
#include "SdpInfo.h"
#include "rtp/RtpHeaders.h"

namespace erizo {
  DEFINE_LOGGER(WebRtcConnection, "WebRtcConnection");
  
  WebRtcConnection::WebRtcConnection(bool audioEnabled, bool videoEnabled, const std::string &stunServer, int stunPort, int minPort, int maxPort, bool trickleEnabled, WebRtcConnectionEventListener* listener)
      : connEventListener_(listener), fec_receiver_(this){
    ELOG_WARN("WebRtcConnection constructor stunserver %s stunPort %d minPort %d maxPort %d\n", stunServer.c_str(), stunPort, minPort, maxPort);
    sequenceNumberFIR_ = 0;
    bundle_ = false;
    this->setVideoSinkSSRC(55543);
    this->setAudioSinkSSRC(44444);
    videoSink_ = NULL;
    audioSink_ = NULL;
    fbSink_ = NULL;
    sourcefbSink_ = this;
    sinkfbSource_ = this;
    globalState_ = CONN_INITIAL;
    videoTransport_ = NULL;
    audioTransport_ = NULL;

    shouldSendFeedback_ = true;

    audioEnabled_ = audioEnabled;
    videoEnabled_ = videoEnabled;
    trickleEnabled_ = trickleEnabled;

    stunServer_ = stunServer;
    stunPort_ = stunPort;
    minPort_ = minPort;
    maxPort_ = maxPort;

    gettimeofday(&mark_, NULL);

    rateControl_ = 0;
     
    sending_ = true;
    rtcpProcessor_ = boost::shared_ptr<RtcpProcessor> (new RtcpProcessor((MediaSink*)this, (MediaSource*) this));
    send_Thread_ = boost::thread(&WebRtcConnection::sendLoop, this);
  }

  WebRtcConnection::~WebRtcConnection() {
    ELOG_INFO("WebRtcConnection Destructor");
    sending_ = false;
    cond_.notify_one();
    send_Thread_.join();
    globalState_ = CONN_FINISHED;
    if (connEventListener_ != NULL){
      connEventListener_->notifyEvent(globalState_, "");
      connEventListener_ = NULL;
    }
    globalState_ = CONN_FINISHED;
    videoSink_ = NULL;
    audioSink_ = NULL;
    fbSink_ = NULL;
    delete videoTransport_;
    videoTransport_=NULL;
    delete audioTransport_;
    audioTransport_= NULL;
  }

  bool WebRtcConnection::init() {
    return true;
  }
  
  bool WebRtcConnection::setRemoteSdp(const std::string &sdp) {
    ELOG_DEBUG("Set Remote SDP %s", sdp.c_str());
    remoteSdp_.initWithSdp(sdp, "");

    bundle_ = remoteSdp_.isBundle;
    ELOG_DEBUG("Is bundle? %d", bundle_);
    localSdp_.setOfferSdp(remoteSdp_);
        
    ELOG_DEBUG("Video %d videossrc %u Audio %d audio ssrc %u Bundle %d", remoteSdp_.hasVideo, remoteSdp_.videoSsrc, remoteSdp_.hasAudio, remoteSdp_.audioSsrc,  bundle_);

    ELOG_DEBUG("Setting SSRC to localSdp %u", this->getVideoSinkSSRC());

    localSdp_.videoSsrc = this->getVideoSinkSSRC();
    localSdp_.audioSsrc = this->getAudioSinkSSRC();

    this->setVideoSourceSSRC(remoteSdp_.videoSsrc);
    this->thisStats_.setVideoSourceSSRC(this->getVideoSourceSSRC());
    this->setAudioSourceSSRC(remoteSdp_.audioSsrc);
    this->thisStats_.setAudioSourceSSRC(this->getAudioSourceSSRC());
    rtcpProcessor_->addSourceSsrc(this->getAudioSourceSSRC());
    rtcpProcessor_->addSourceSsrc(this->getVideoSourceSSRC());

    if (!videoTransport_ && !audioTransport_){ // For now we don't re/check transports, if they are already created we leave them there
      if (remoteSdp_.profile == SAVPF) {
        if (remoteSdp_.isFingerprint) {
          if (remoteSdp_.hasVideo||bundle_) {
            std::string username, password;
            remoteSdp_.getCredentials(username, password, VIDEO_TYPE);
            videoTransport_ = new DtlsTransport(VIDEO_TYPE, "video", bundle_, remoteSdp_.isRtcpMux, this, stunServer_, stunPort_, minPort_, maxPort_, username, password);
          }
          if (!bundle_ && remoteSdp_.hasAudio) {
            std::string username, password;
            remoteSdp_.getCredentials(username, password, AUDIO_TYPE);
            audioTransport_ = new DtlsTransport(AUDIO_TYPE, "audio", bundle_, remoteSdp_.isRtcpMux, this, stunServer_, stunPort_, minPort_, maxPort_, username, password);
          }
        }
      }
    }
    
    if(trickleEnabled_){
      std::string object = this->getLocalSdp();
      if (connEventListener_){
        connEventListener_->notifyEvent(CONN_SDP, object);
      }
    }

    if (!remoteSdp_.getCandidateInfos().empty()){
      ELOG_DEBUG("There are candidate in the SDP: Setting Remote Candidates");
      if (remoteSdp_.hasVideo) {
        videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos(), bundle_);
      }
      if (!bundle_ && remoteSdp_.hasAudio) {
        audioTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos(), bundle_);
      }
    }

    if (remoteSdp_.videoBandwidth !=0){
      ELOG_DEBUG("Setting remote bandwidth %u", remoteSdp_.videoBandwidth);
      this->rtcpProcessor_->setVideoBW(remoteSdp_.videoBandwidth*1000);
    }

    return true;
  }

  bool WebRtcConnection::addRemoteCandidate(const std::string &mid, int mLineIndex, const std::string &sdp) {
    // TODO Check type of transport.
    ELOG_DEBUG("Adding remote Candidate %s, mid %s, sdpMLine %d",sdp.c_str(), mid.c_str(), mLineIndex);
    MediaType theType;
    std::string theMid;
    if ((!mid.compare("video"))||(mLineIndex ==remoteSdp_.videoSdpMLine)){
      theType = VIDEO_TYPE;
      theMid = "video";
    }else{
      theType = AUDIO_TYPE;
      theMid = "audio";
    }
    SdpInfo tempSdp;
    std::string username, password;
    remoteSdp_.getCredentials(username, password, theType);
    tempSdp.setCredentials(username, password, OTHER);
    bool res = false;
    if(tempSdp.initWithSdp(sdp, theMid)){
      if (theType == VIDEO_TYPE||bundle_){
        ELOG_DEBUG("Setting VIDEO CANDIDATE" );
        res = videoTransport_->setRemoteCandidates(tempSdp.getCandidateInfos(), bundle_);
      } else if (theType==AUDIO_TYPE){
        ELOG_DEBUG("Setting AUDIO CANDIDATE");
        res = audioTransport_->setRemoteCandidates(tempSdp.getCandidateInfos(), bundle_);
      }else{
        ELOG_ERROR("Cannot add remote candidate with no Media (video or audio)");
      }
    }

    for (uint8_t it = 0; it < tempSdp.getCandidateInfos().size(); it++){
      remoteSdp_.addCandidate(tempSdp.getCandidateInfos()[it]);
    }
    return res;
  }

  std::string WebRtcConnection::getLocalSdp() {
    ELOG_DEBUG("Getting SDP");
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
    char lineIndex[1];
    sprintf(lineIndex,"%d",(mid.compare("video")?localSdp_.audioSdpMLine:localSdp_.videoSdpMLine));
    object["sdpMLineIndex"] = std::string(lineIndex);

    std::ostringstream theString;
    theString << "{";
    for (std::map<std::string, std::string>::iterator it=object.begin(); it!=object.end(); ++it){
      theString << "\"" << it->first << "\":\"" << it->second << "\"";
      if (++it != object.end()){
        theString << ",";
      }
      --it;
    }
    theString << "}";
    return theString.str();
  }

  void WebRtcConnection::onCandidate(const CandidateInfo& cand, Transport *transport) {
    std::string sdp = localSdp_.addCandidate(cand);
    ELOG_DEBUG("On Candidate %s", sdp.c_str());
    if(trickleEnabled_){
      if (connEventListener_ != NULL) {
        if (!bundle_) {
          std::string object = this->getJSONCandidate(transport->transport_name, sdp);
          connEventListener_->notifyEvent(CONN_CANDIDATE, object);
        } else {
          if (remoteSdp_.hasAudio){
            std::string object = this->getJSONCandidate("audio", sdp);
            connEventListener_->notifyEvent(CONN_CANDIDATE, object);
          }
          if (remoteSdp_.hasVideo){
            std::string object2 = this->getJSONCandidate("video", sdp);
            connEventListener_->notifyEvent(CONN_CANDIDATE, object2);
          }
        }
        
      }
    }
  }

  int WebRtcConnection::deliverAudioData_(char* buf, int len) {
    if (bundle_){
      if (videoTransport_ != NULL) {
        if (audioEnabled_ == true) {
          this->queueData(0, buf, len, videoTransport_, AUDIO_PACKET);
        }
      }
    } else if (audioTransport_ != NULL) {
      if (audioEnabled_ == true) {
        this->queueData(0, buf, len, audioTransport_, AUDIO_PACKET);
      }
    }
    return len;
  }


  // This is called by our fec_ object when it recovers a packet.
  bool WebRtcConnection::OnRecoveredPacket(const uint8_t* rtp_packet, int rtp_packet_length) {
      this->queueData(0, (const char*) rtp_packet, rtp_packet_length, videoTransport_, VIDEO_PACKET);
      return true;
  }

  int32_t WebRtcConnection::OnReceivedPayloadData(const uint8_t* /*payload_data*/, const uint16_t /*payload_size*/, const webrtc::WebRtcRTPHeader* /*rtp_header*/) {
      // Unused by WebRTC's FEC implementation; just something we have to implement.
      return 0;
  }

  int WebRtcConnection::deliverVideoData_(char* buf, int len) {
    if (videoTransport_ != NULL) {
      if (videoEnabled_ == true) {
  
        RtpHeader* h = reinterpret_cast<RtpHeader*>(buf);
        if (h->getPayloadType() == RED_90000_PT && !remoteSdp_.supportPayloadType(RED_90000_PT)) {
          // This is a RED/FEC payload, but our remote endpoint doesn't support that (most likely because it's firefox :/ )
          // Let's go ahead and run this through our fec receiver to convert it to raw VP8
          webrtc::RTPHeader hackyHeader;
          hackyHeader.headerLength = h->getHeaderLength();
          hackyHeader.sequenceNumber = h->getSeqNumber();
          // FEC copies memory, manages its own memory, including memory passed in callbacks (in the callback, be sure to memcpy out of webrtc's buffers
          if (fec_receiver_.AddReceivedRedPacket(hackyHeader, (const uint8_t*) buf, len, ULP_90000_PT) == 0) {
            fec_receiver_.ProcessReceivedFec();
          }
        } else {
          this->queueData(0, buf, len, videoTransport_, VIDEO_PACKET);
        }
      }
    }
    return len;
  }

  int WebRtcConnection::deliverFeedback_(char* buf, int len){
    // Check where to send the feedback

    rtcpProcessor_->analyzeFeedback(buf,len);
    return len;
  }

  void WebRtcConnection::writeSsrc(char* buf, int len, unsigned int ssrc) {
    ELOG_DEBUG("LEN %d", len);
    RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
    //if it is RTCP we check it it is a compound packet
    if (chead->isRtcp()) {      
      char* movingBuf = buf;
      int rtcpLength = 0;
      int totalLength = 0;
      do{
        movingBuf+=rtcpLength;
        RtcpHeader *chead= reinterpret_cast<RtcpHeader*>(movingBuf);
        rtcpLength= (ntohs(chead->length)+1)*4;      
        totalLength+= rtcpLength;
        ELOG_DEBUG("Is RTCP, prev SSRC %u, new %u, len %d ", chead->getSSRC(), ssrc, rtcpLength);
        chead->ssrc=htonl(ssrc);
        if (chead->packettype == RTCP_PS_Feedback_PT){
          FirHeader *thefir = reinterpret_cast<FirHeader*>(movingBuf);
          if (thefir->fmt == 4){ // It is a FIR Packet, we generate it
            this->sendPLI();
          }
        }
      } while(totalLength<len);
    } else {
      head->setSSRC(ssrc);
    }
  }

  void WebRtcConnection::onTransportData(char* buf, int len, Transport *transport) {
    if (audioSink_ == NULL && videoSink_ == NULL && fbSink_==NULL){
      return;
    }
    
    // PROCESS RTCP
    RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(buf);
    if (chead->isRtcp()) {
      thisStats_.processRtcpPacket(buf, len);
      if (chead->packettype == RTCP_Sender_PT) { //Sender Report
        rtcpProcessor_->analyzeSr(chead);
      }
    }

    // DELIVER FEEDBACK (RR, FEEDBACK PACKETS)
    if (chead->isFeedback()){
      if (fbSink_ != NULL && shouldSendFeedback_) {
        fbSink_->deliverFeedback(buf,len);
      }
    } else {
      // RTP or RTCP Sender Report
      if (bundle_) {
        // Check incoming SSRC
        RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
        RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
        uint32_t recvSSRC;
        if (chead->packettype == RTCP_Sender_PT) { //Sender Report
          recvSSRC = chead->getSSRC();             
        }else{
          recvSSRC = head->getSSRC();
        }
        // Deliver data
        if (recvSSRC==this->getVideoSourceSSRC()) {
          parseIncomingPayloadType(buf, len, VIDEO_PACKET);
          videoSink_->deliverVideoData(buf, len);
        } else if (recvSSRC==this->getAudioSourceSSRC()) {
          parseIncomingPayloadType(buf, len, AUDIO_PACKET);
          audioSink_->deliverAudioData(buf, len);
        } else {
          ELOG_ERROR("Unknown SSRC %u, localVideo %u, remoteVideo %u, ignoring", recvSSRC, this->getVideoSourceSSRC(), this->getVideoSinkSSRC());
        }
      } else if (transport->mediaType == AUDIO_TYPE) {
        if (audioSink_ != NULL) {
          parseIncomingPayloadType(buf, len, AUDIO_PACKET);
          RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
          RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
          // Firefox does not send SSRC in SDP
          if (this->getAudioSourceSSRC() == 0) {
            unsigned int recvSSRC;
            this->setAudioSourceSSRC(head->getSSRC());		
            if (chead->packettype == RTCP_Sender_PT) { // Sender Report
              recvSSRC = chead->getSSRC();
            } else {
              recvSSRC = head->getSSRC();
            }
            ELOG_DEBUG("Audio Source SSRC is %u", recvSSRC);
            this->setAudioSourceSSRC(recvSSRC);
          }
          audioSink_->deliverAudioData(buf, len);
        }
      } else if (transport->mediaType == VIDEO_TYPE) {
        if (videoSink_ != NULL) {
          parseIncomingPayloadType(buf, len, VIDEO_PACKET);
          RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
          RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
           // Firefox does not send SSRC in SDP
          if (this->getVideoSourceSSRC() == 0) {
            unsigned int recvSSRC;
            if (chead->packettype == RTCP_Sender_PT) { //Sender Report
              recvSSRC = chead->getSSRC();
            } else {
              recvSSRC = head->getSSRC();
            }
            ELOG_DEBUG("Video Source SSRC is %u", recvSSRC);
            this->setVideoSourceSSRC(recvSSRC);
          }
          // change ssrc for RTP packets, don't touch here if RTCP
          videoSink_->deliverVideoData(buf, len);
        }
      }
    }
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
    int len = (thePLI.getLength()+1)*4;
    this->queueData(0, buf, len , videoTransport_, OTHER_PACKET);
    return len; 
    
  }

  
     
  void WebRtcConnection::updateState(TransportState state, Transport * transport) {
    boost::mutex::scoped_lock lock(updateStateMutex_);
    WebRTCEvent temp = globalState_;
    std::string msg = "";
    ELOG_INFO("Update Transport State %s to %d", transport->transport_name.c_str(), state);
    if (videoTransport_ == NULL && audioTransport_ == NULL) {
      ELOG_ERROR("Update Transport State with Transport NULL, this should not happen!");
      return;
    }
    

    if (globalState_ == CONN_FAILED) {
      // if current state is failed -> noop
      return;
    }

    switch (state){
      case TRANSPORT_STARTED:
        if (bundle_){
          temp = CONN_STARTED;
        }else{
          if ((!remoteSdp_.hasAudio || (audioTransport_ != NULL && audioTransport_->getTransportState() == TRANSPORT_STARTED)) &&
            (!remoteSdp_.hasVideo || (videoTransport_ != NULL && videoTransport_->getTransportState() == TRANSPORT_STARTED))) {
              // WebRTCConnection will be ready only when all channels are ready.
              temp = CONN_STARTED;
            }
        }
        break;
      case TRANSPORT_GATHERED:
        if (bundle_){
          if(!trickleEnabled_){
            temp = CONN_GATHERED;
            msg = this->getLocalSdp();
          }
        }else{
          if ((!remoteSdp_.hasAudio || (audioTransport_ != NULL && audioTransport_->getTransportState() == TRANSPORT_GATHERED)) &&
            (!remoteSdp_.hasVideo || (videoTransport_ != NULL && videoTransport_->getTransportState() == TRANSPORT_GATHERED))) {
              // WebRTCConnection will be ready only when all channels are ready.
              if(!trickleEnabled_){
                temp = CONN_GATHERED;
                msg = this->getLocalSdp();
              }
            }
        }
        break;
      case TRANSPORT_READY:
        if (bundle_){
          temp = CONN_READY;

        }else{
          if ((!remoteSdp_.hasAudio || (audioTransport_ != NULL && audioTransport_->getTransportState() == TRANSPORT_READY)) &&
            (!remoteSdp_.hasVideo || (videoTransport_ != NULL && videoTransport_->getTransportState() == TRANSPORT_READY))) {
              // WebRTCConnection will be ready only when all channels are ready.
              temp = CONN_READY;            
            }
        }
        break;
      case TRANSPORT_FAILED:
        temp = CONN_FAILED;
        sending_ = false;
        msg = remoteSdp_.getSdp();
        ELOG_INFO("WebRtcConnection failed, stopping sending");
        cond_.notify_one();
        break;
      default:
        ELOG_DEBUG("New state %d", state);
        break;
    }

    if (audioTransport_ != NULL && videoTransport_ != NULL) {
      ELOG_INFO("%s - Update Transport State end, %d - %d, %d - %d, %d - %d", 
        transport->transport_name.c_str(),
        (int)audioTransport_->getTransportState(), 
        (int)videoTransport_->getTransportState(), 
        this->getAudioSourceSSRC(),
        this->getVideoSourceSSRC(),
        (int)temp, 
        (int)globalState_);
    }
    
    if (globalState_ == temp)
      return;

    globalState_ = temp;

    if (connEventListener_ != NULL) {
      connEventListener_->notifyEvent(globalState_, msg);
    }
  }
   // changes the outgoing payload type for in the given data packet
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
//            ELOG_ERROR("onTransportData mapping %i to %i", externalPT, internalPT);
        } else {
//            ELOG_ERROR("onTransportData did not find mapping for %i", externalPT);
        }
      }
  }


  void WebRtcConnection::queueData(int comp, const char* buf, int length, Transport *transport, packetType type) {
    if ((audioSink_ == NULL && videoSink_ == NULL && fbSink_==NULL) || !sending_) //we don't enqueue data if there is nothing to receive it
      return;
    boost::mutex::scoped_lock lock(receiveVideoMutex_);
    if (!sending_)
      return;
    if (comp == -1){
      sending_ = false;
      std::queue<dataPacket> empty;
      std::swap( sendQueue_, empty);
      dataPacket p_;
      p_.comp = -1;
      sendQueue_.push(p_);
      cond_.notify_one();
      return;
    }
    if (sendQueue_.size() < 1000) {
      dataPacket p_;
      memcpy(p_.data, buf, length);
      p_.comp = comp;
//      p_.type = (transport->mediaType == VIDEO_TYPE) ? VIDEO_PACKET : AUDIO_PACKET;
      p_.type = type;
      p_.length = length;
      changeDeliverPayloadType(&p_, type);
      sendQueue_.push(p_);
    }else{
      ELOG_DEBUG("Discarding Packets");
    }
    cond_.notify_one();
  }

  WebRTCEvent WebRtcConnection::getCurrentState() {
    return globalState_;
  }

  std::string WebRtcConnection::getJSONStats(){
    return thisStats_.getStats();
  }

  void WebRtcConnection::sendLoop() {
    uint32_t partial_bitrate = 0;
    uint64_t sentVideoBytes = 0;
    uint64_t lastSecondVideoBytes = 0;
      while (sending_) {
          dataPacket p;
          {
              boost::unique_lock<boost::mutex> lock(receiveVideoMutex_);
              while (sendQueue_.size() == 0) {
                  cond_.wait(lock);
                  if (!sending_) {
                      return;
                  }
              }
              if(sendQueue_.front().comp ==-1){
                  sending_ =  false;
                  ELOG_DEBUG("Finishing send Thread, packet -1");
                  sendQueue_.pop();
                  return;
              }

              p = sendQueue_.front();
              sendQueue_.pop();
          }

          if (bundle_ || p.type == VIDEO_PACKET) {
            if (rateControl_){
              if (p.type == VIDEO_PACKET){
                if (rateControl_ == 1)
                  continue;
                gettimeofday(&now_, NULL);
                uint64_t nowms = (now_.tv_sec * 1000) + (now_.tv_usec / 1000);
                uint64_t markms = (mark_.tv_sec * 1000) + (mark_.tv_usec/1000);
                if ((nowms - markms)>=100){
                  mark_ = now_;
                  lastSecondVideoBytes = sentVideoBytes;
                }
                partial_bitrate = ((sentVideoBytes - lastSecondVideoBytes)*8)*10;
                if (partial_bitrate > this->rateControl_){
                  continue;
                }
                sentVideoBytes+=p.length;
              }
            }
              videoTransport_->write(p.data, p.length);
          } else {
              audioTransport_->write(p.data, p.length);
          }
      }
  }
}

/* namespace erizo */
