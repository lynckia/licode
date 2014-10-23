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

  WebRtcConnection::WebRtcConnection(bool audioEnabled, bool videoEnabled, const std::string &stunServer, int stunPort, int minPort, int maxPort, WebRtcConnectionEventListener* listener)
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

    audioEnabled_ = audioEnabled;
    videoEnabled_ = videoEnabled;

    stunServer_ = stunServer;
    stunPort_ = stunPort;
    minPort_ = minPort;
    maxPort_ = maxPort;
    
    sending_ = true;
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
    //std::vector<CryptoInfo> crypto_remote = remoteSdp_.getCryptoInfos();
    int video = (remoteSdp_.videoSsrc==0?false:true);
    int audio = (remoteSdp_.audioSsrc==0?false:true);

    bundle_ = remoteSdp_.isBundle;
    ELOG_DEBUG("Is bundle? %d %d ", bundle_, true);
    localSdp_.getPayloadInfos() = remoteSdp_.getPayloadInfos();
    localSdp_.isBundle = bundle_;
    localSdp_.isRtcpMux = remoteSdp_.isRtcpMux;

    ELOG_DEBUG("Video %d videossrc %u Audio %d audio ssrc %u Bundle %d", video, remoteSdp_.videoSsrc, audio, remoteSdp_.audioSsrc,  bundle_);

    ELOG_DEBUG("Setting SSRC to localSdp %u", this->getVideoSinkSSRC());
    localSdp_.videoSsrc = this->getVideoSinkSSRC();
    localSdp_.audioSsrc = this->getAudioSinkSSRC();

    this->setVideoSourceSSRC(remoteSdp_.videoSsrc);
    this->thisStats_.setVideoSourceSSRC(this->getVideoSourceSSRC());
    this->setAudioSourceSSRC(remoteSdp_.audioSsrc);
    this->thisStats_.setAudioSourceSSRC(this->getAudioSourceSSRC());

    if (remoteSdp_.profile == SAVPF) {
      if (remoteSdp_.isFingerprint) {
        // DTLS-SRTP
        if (remoteSdp_.hasVideo) {
          videoTransport_ = new DtlsTransport(VIDEO_TYPE, "video", bundle_, remoteSdp_.isRtcpMux, this, stunServer_, stunPort_, minPort_, maxPort_);
        }
        if (!bundle_ && remoteSdp_.hasAudio) {
          audioTransport_ = new DtlsTransport(AUDIO_TYPE, "audio", bundle_, remoteSdp_.isRtcpMux, this, stunServer_, stunPort_, minPort_, maxPort_);
        }
      }
    }
    std::string object = this->getLocalSdp();
    if (!remoteSdp_.getCandidateInfos().empty()){
      if (remoteSdp_.hasVideo) {
        videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
      }
      if (!bundle_ && remoteSdp_.hasAudio) {
        audioTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
      }
    }

    if (connEventListener_){
      connEventListener_->notifyEvent(CONN_SDP, object);
    }

    return true;
  }

  bool WebRtcConnection::addRemoteCandidate(const std::string &mid, const std::string &sdp) {
    // TODO Check type of transport.
    SdpInfo tempSdp;
    std::string username, password;
    remoteSdp_.getCredentials(username, password, VIDEO_TYPE);
    tempSdp.setCredentials(username, password, VIDEO_TYPE);
    tempSdp.initWithSdp(sdp, mid);
    bool res;
    if (mid == "video") {
      res = videoTransport_->setRemoteCandidates(tempSdp.getCandidateInfos());
    } else if (!bundle_ && mid == "audio") {
      res = audioTransport_->setRemoteCandidates(tempSdp.getCandidateInfos());
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

  void WebRtcConnection::onCandidate(const std::string& sdp, Transport *transport) {
    if (connEventListener_ != NULL) {
      if (!bundle_) {
        std::string object = this->getJSONCandidate(transport->transport_name, sdp);
        connEventListener_->notifyEvent(CONN_CANDIDATE, object);
      } else {
        std::string object = this->getJSONCandidate("audio", sdp);
        connEventListener_->notifyEvent(CONN_CANDIDATE, object);
        std::string object2 = this->getJSONCandidate("video", sdp);
        connEventListener_->notifyEvent(CONN_CANDIDATE, object2);
      }
      
    }
  }

  int WebRtcConnection::deliverAudioData_(char* buf, int len) {
    writeSsrc(buf, len, this->getAudioSinkSSRC());
    if (bundle_){
      if (videoTransport_ != NULL) {
        if (audioEnabled_ == true) {
          this->queueData(0, buf, len, videoTransport_);
        }
      }
    } else if (audioTransport_ != NULL) {
      if (audioEnabled_ == true) {
        this->queueData(0, buf, len, audioTransport_);
      }
    }
    return len;
  }


  // This is called by our fec_ object when it recovers a packet.
  bool WebRtcConnection::OnRecoveredPacket(const uint8_t* rtp_packet, int rtp_packet_length) {
      this->queueData(0, (const char*) rtp_packet, rtp_packet_length, videoTransport_);
      return true;
  }

  int32_t WebRtcConnection::OnReceivedPayloadData(const uint8_t* /*payload_data*/, const uint16_t /*payload_size*/, const webrtc::WebRtcRTPHeader* /*rtp_header*/) {
      // Unused by WebRTC's FEC implementation; just something we have to implement.
      return 0;
  }

  int WebRtcConnection::deliverVideoData_(char* buf, int len) {
    writeSsrc(buf, len, this->getVideoSinkSSRC());
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
              this->queueData(0, buf, len, videoTransport_);
          }
      }
    }
    return len;
  }

  int WebRtcConnection::deliverFeedback_(char* buf, int len){
    // Check where to send the feedback
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
//    ELOG_DEBUG("received Feedback type %u ssrc %u, sourcessrc %u", chead->packettype, chead->getSSRC(), chead->getSourceSSRC());
    if (chead->getSourceSSRC() == this->getAudioSourceSSRC()) {
        writeSsrc(buf,len,this->getAudioSinkSSRC());
    } else {
        writeSsrc(buf,len,this->getVideoSinkSSRC());      
    }

    if (videoTransport_ != NULL) {
      this->queueData(0, buf, len, videoTransport_);
    }
    return len;
  }

  void WebRtcConnection::writeSsrc(char* buf, int len, unsigned int ssrc) {
    RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
    //if it is RTCP we check it it is a compound packet
    if (chead->isRtcp()) {
        processRtcpHeaders(buf,len,ssrc);
    } else {
      head->ssrc=htonl(ssrc);
    }
  }

  void WebRtcConnection::onTransportData(char* buf, int len, Transport *transport) {
    if (audioSink_ == NULL && videoSink_ == NULL && fbSink_==NULL){
      return;
    }
    
    // PROCESS STATS
    if (this->statsListener_){ // if there is no listener we dont process stats
      RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
      if (head->payloadtype != RED_90000_PT && head->payloadtype != PCMU_8000_PT)     
        thisStats_.processRtcpPacket(buf, len);
    }
    RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(buf);
    // DELIVER FEEDBACK (RR, FEEDBACK PACKETS)
    if (chead->isFeedback()){
      if (fbSink_ != NULL) {
        fbSink_->deliverFeedback(buf,len);
      }
    } else {
      // RTP or RTCP Sender Report
      if (bundle_) {
        // Check incoming SSRC
        RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
        RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (buf);
        unsigned int recvSSRC;
        if (chead->packettype == RTCP_Sender_PT) { //Sender Report
          recvSSRC = chead->getSSRC();
        }else{
          recvSSRC = head->getSSRC();
        }
        // Deliver data
        if (recvSSRC==this->getVideoSourceSSRC() || recvSSRC==this->getVideoSinkSSRC()) {
          videoSink_->deliverVideoData(buf, len);
        } else if (recvSSRC==this->getAudioSourceSSRC() || recvSSRC==this->getAudioSinkSSRC()) {
          audioSink_->deliverAudioData(buf, len);
        } else {
          ELOG_ERROR("Unknown SSRC %u, localVideo %u, remoteVideo %u, ignoring", recvSSRC, this->getVideoSourceSSRC(), this->getVideoSinkSSRC());
        }
      } else if (transport->mediaType == AUDIO_TYPE) {
        if (audioSink_ != NULL) {
          RtpHeader *head = reinterpret_cast<RtpHeader*> (buf);
          // Firefox does not send SSRC in SDP
          if (this->getAudioSourceSSRC() == 0) {
            ELOG_DEBUG("Audio Source SSRC is %u", head->getSSRC());
            this->setAudioSourceSSRC(head->getSSRC());
          }
          head->setSSRC(this->getAudioSinkSSRC());
          audioSink_->deliverAudioData(buf, len);
        }
      } else if (transport->mediaType == VIDEO_TYPE) {
        if (videoSink_ != NULL) {
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
          if (chead->packettype != RTCP_Sender_PT) {
            head->setSSRC(this->getVideoSinkSSRC());
          }
          videoSink_->deliverVideoData(buf, len);
        }
      }
    }
  }

  int WebRtcConnection::sendFirPacket() {
    ELOG_DEBUG("Generating FIR Packet");
    sequenceNumberFIR_++; // do not increase if repetition
    int pos = 0;
    uint8_t rtcpPacket[50];
    // add full intra request indicator
    uint8_t FMT = 4;
    rtcpPacket[pos++] = (uint8_t) 0x80 + FMT;
    rtcpPacket[pos++] = (uint8_t) 206;

    //Length of 4
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) (4);

    // Add our own SSRC
    uint32_t* ptr = reinterpret_cast<uint32_t*>(rtcpPacket + pos);
    ptr[0] = htonl(this->getVideoSinkSSRC());
    pos += 4;

    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;
    // Additional Feedback Control Information (FCI)
    uint32_t* ptr2 = reinterpret_cast<uint32_t*>(rtcpPacket + pos);
    ptr2[0] = htonl(this->getVideoSourceSSRC());
    pos += 4;

    rtcpPacket[pos++] = (uint8_t) (sequenceNumberFIR_);
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;

    if (videoTransport_ != NULL) {
      if (videoTransport_->getTransportState()!=TRANSPORT_READY)
        ELOG_DEBUG("Sending FIR when not READY %d", videoTransport_->getTransportState());
      videoTransport_->write((char*)rtcpPacket, pos);
    }
    return pos;

  }

  void WebRtcConnection::updateState(TransportState state, Transport * transport) {
    boost::mutex::scoped_lock lock(updateStateMutex_);
    WebRTCEvent temp = globalState_;
    std::string msg = "";
    ELOG_INFO("Update Transport State %s to %d", transport->transport_name.c_str(), state);
    if (audioTransport_ == NULL && videoTransport_ == NULL) {
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
        ELOG_INFO("WebRtcConnection failed, stopping sending");
        cond_.notify_one();
        ELOG_INFO("WebRtcConnection failed, stopped sending");
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

  void WebRtcConnection::queueData(int comp, const char* buf, int length, Transport *transport) {
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
      p_.type = (transport->mediaType == VIDEO_TYPE) ? VIDEO_PACKET : AUDIO_PACKET;
      p_.length = length;
      sendQueue_.push(p_);
    }
    cond_.notify_one();
  }

  WebRTCEvent WebRtcConnection::getCurrentState() {
    return globalState_;
  }

  void WebRtcConnection::processRtcpHeaders(char* buf, int len, unsigned int ssrc){
    char* movingBuf = buf;
    int rtcpLength = 0;
    int totalLength = 0;
    do{
      movingBuf+=rtcpLength;
      RtcpHeader *chead= reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength= (ntohs(chead->length)+1)*4;      
      totalLength+= rtcpLength;
      chead->ssrc=htonl(ssrc);
      if (chead->packettype == RTCP_PS_Feedback_PT){
        FirHeader *thefir = reinterpret_cast<FirHeader*>(movingBuf);
        if (thefir->fmt == 4){ // It is a FIR Packet, we generate it
          //ELOG_DEBUG("Feedback FIR packet, changed source %u sourcessrc to %u fmt %d", ssrc, sourcessrc, thefir->fmt);
          this->sendFirPacket();
        }
      }
    } while(totalLength<len);
  }

  void WebRtcConnection::sendLoop() {
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
              videoTransport_->write(p.data, p.length);
          } else {
              audioTransport_->write(p.data, p.length);
          }
      }
  }
}
/* namespace erizo */
