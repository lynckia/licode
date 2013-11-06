/*
 * WebRTCConnection.cpp
 */

#include <cstdio>

#include "WebRtcConnection.h"
#include "DtlsTransport.h"
#include "SdesTransport.h"

#include "SdpInfo.h"
#include "rtputils.h"

namespace erizo {
  DEFINE_LOGGER(WebRtcConnection, "WebRtcConnection");

  WebRtcConnection::WebRtcConnection(bool audioEnabled, bool videoEnabled, const std::string &stunServer, int stunPort, int minPort, int maxPort) {

    ELOG_WARN("WebRtcConnection constructor stunserver %s stunPort %d minPort %d maxPort %d\n", stunServer.c_str(), stunPort, minPort, maxPort);
    video_ = 0;
    audio_ = 0;
    sequenceNumberFIR_ = 0;
    bundle_ = false;
    this->setVideoSinkSSRC(55543);
    this->setAudioSinkSSRC(44444);
    videoSink_ = NULL;
    audioSink_ = NULL;
    fbSink_ = NULL;
    sourcefbSink_ = this;
    sinkfbSource_ = this;

    globalState_ = INITIAL;
    connStateListener_ = NULL;

    sending_ = true;
    send_Thread_ = boost::thread(&WebRtcConnection::sendLoop, this);

    videoTransport_ = NULL;
    audioTransport_ = NULL;

    audioEnabled_ = audioEnabled;
    videoEnabled_ = videoEnabled;

    deliverMediaBuffer_ = (char*)malloc(3000);

    stunServer_ = stunServer;
    stunPort_ = stunPort;
    minPort_ = minPort;
    maxPort_ = maxPort;
  }

  WebRtcConnection::~WebRtcConnection() {
    ELOG_DEBUG("WebRtcConnection Destructor");
    this->close();
    free(deliverMediaBuffer_);
  }

  bool WebRtcConnection::init() {
    return true;
  }

  void WebRtcConnection::close() {
    ELOG_DEBUG("WebRtcConnection Close");
    sending_ = false;
    delete videoTransport_;
    videoTransport_=NULL;
    delete audioTransport_;
    audioTransport_= NULL;
    send_Thread_.join();
  }

  void WebRtcConnection::closeSink(){
    ELOG_WARN ("closeSink");
    this->close();
  }

  void WebRtcConnection::closeSource(){
    this->close();
  }

  bool WebRtcConnection::setRemoteSdp(const std::string &sdp) {
    ELOG_DEBUG("Set Remote SDP %s", sdp.c_str());
    remoteSdp_.initWithSdp(sdp);
    //std::vector<CryptoInfo> crypto_remote = remoteSdp_.getCryptoInfos();
    video_ = (remoteSdp_.videoSsrc==0?false:true);
    audio_ = (remoteSdp_.audioSsrc==0?false:true);

    CryptoInfo cryptLocal_video;
    CryptoInfo cryptLocal_audio;
    CryptoInfo cryptRemote_video;
    CryptoInfo cryptRemote_audio;

    bundle_ = remoteSdp_.isBundle;
    ELOG_DEBUG("Is bundle? %d %d ", bundle_, true);
    std::vector<RtpMap> payloadRemote = remoteSdp_.getPayloadInfos();
    localSdp_.getPayloadInfos() = remoteSdp_.getPayloadInfos();
    localSdp_.isBundle = bundle_;
    localSdp_.isRtcpMux = remoteSdp_.isRtcpMux;

    ELOG_DEBUG("Video %d videossrc %u Audio %d audio ssrc %u Bundle %d", video_, remoteSdp_.videoSsrc, audio_, remoteSdp_.audioSsrc,  bundle_);

    ELOG_DEBUG("Setting SSRC to localSdp %u", this->getVideoSinkSSRC());
    localSdp_.videoSsrc = this->getVideoSinkSSRC();
    localSdp_.audioSsrc = this->getAudioSinkSSRC();

    this->setVideoSourceSSRC(remoteSdp_.videoSsrc);
    this->setAudioSourceSSRC(remoteSdp_.audioSsrc);

    if (remoteSdp_.profile == SAVPF) {
      if (remoteSdp_.isFingerprint) {
        // DTLS-SRTP
        if (remoteSdp_.hasVideo) {
          videoTransport_ = new DtlsTransport(VIDEO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, this, stunServer_, stunPort_, minPort_, maxPort_);
        }
        if (remoteSdp_.hasAudio) {
          audioTransport_ = new DtlsTransport(AUDIO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, this, stunServer_, stunPort_, minPort_, maxPort_);
        }
      } else {
        // SDES
        std::vector<CryptoInfo> crypto_remote = remoteSdp_.getCryptoInfos();
        for (unsigned int it = 0; it < crypto_remote.size(); it++) {
          CryptoInfo cryptemp = crypto_remote[it];
          if (cryptemp.mediaType == VIDEO_TYPE
              && !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
            videoTransport_ = new SdesTransport(VIDEO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, &cryptemp, this, stunServer_, stunPort_, minPort_, maxPort_);
          } else if (!bundle_ && cryptemp.mediaType == AUDIO_TYPE
              && !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
            audioTransport_ = new SdesTransport(AUDIO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, &cryptemp, this, stunServer_, stunPort_, minPort_, maxPort_);
          }
        }
      }
    }

    return true;
  }

  std::string WebRtcConnection::getLocalSdp() {
    ELOG_DEBUG("Getting SDP");
    if (videoTransport_ != NULL) {
      videoTransport_->processLocalSdp(&localSdp_);
    }
    ELOG_DEBUG("Video SDP done.");
    if (!bundle_ && audioTransport_ != NULL) {
      audioTransport_->processLocalSdp(&localSdp_);
    }
    ELOG_DEBUG("Audio SDP done.");
    localSdp_.profile = remoteSdp_.profile;
    return localSdp_.getSdp();
  }

  int WebRtcConnection::deliverAudioData(char* buf, int len) {
    boost::mutex::scoped_lock lock(receiveAudioMutex_);
    writeSsrc(buf, len, this->getAudioSinkSSRC());
    if (bundle_){
      if (videoTransport_ != NULL) {
        if (audioEnabled_ == true) {
          videoTransport_->write(buf, len);
        }
      }
    } else if (audioTransport_ != NULL) {
      if (audioEnabled_ == true) {
        audioTransport_->write(buf, len);
      }
    }
    return len;
  }

  int WebRtcConnection::deliverVideoData(char* buf, int len) {
    boost::mutex::scoped_lock lock(receiveAudioMutex_);
    rtpheader *head = (rtpheader*) buf;

    if (head->payloadtype == RED_90000_PT) {
      int totalLength = 12;

      if (head->extension) {
        totalLength += ntohs(head->extensionlength)*4 + 4; // RTP Extension header
      }
      int rtpHeaderLength = totalLength;
      redheader *redhead = (redheader*) (buf + totalLength);

      //redhead->payloadtype = remoteSdp_.inOutPTMap[redhead->payloadtype];
      if (!remoteSdp_.supportPayloadType(head->payloadtype)) {
        while (redhead->follow) {
          totalLength += redhead->getLength() + 4; // RED header
          redhead = (redheader*) (buf + totalLength);
        }
        // Parse RED packet to VP8 packet.
        // Copy RTP header
        memcpy(deliverMediaBuffer_, buf, rtpHeaderLength);
        // Copy payload data
        memcpy(deliverMediaBuffer_ + totalLength, buf + totalLength + 1, len - totalLength - 1);
        // Copy payload type
        rtpheader *mediahead = (rtpheader*) deliverMediaBuffer_;
        mediahead->payloadtype = redhead->payloadtype;
        buf = deliverMediaBuffer_;
        len = len - 1 - totalLength + rtpHeaderLength;
      }
    }
    writeSsrc(buf, len, this->getVideoSinkSSRC());
    if (videoTransport_ != NULL) {
      if (videoEnabled_ == true) {
        videoTransport_->write(buf, len);
      }
    }
    return len;
  }

  int WebRtcConnection::deliverFeedback(char* buf, int len){
    // Check where to send the feedback
    rtcpheader *chead = (rtcpheader*) buf;
    ELOG_DEBUG("received Feedback type %u ssrc %u, sourcessrc %u", chead->packettype, ntohl(chead->ssrc), ntohl(chead->ssrcsource));
    if (ntohl(chead->ssrcsource) == this->getAudioSourceSSRC()) {
        writeSsrc(buf,len,this->getAudioSinkSSRC());      
    } else {
        writeSsrc(buf,len,this->getVideoSinkSSRC());      
    }

    if (bundle_){
      if (videoTransport_ != NULL) {
        videoTransport_->write(buf, len);
      }
    } else {
      // TODO: Check where to send the feedback
      if (videoTransport_ != NULL) {
        videoTransport_->write(buf, len);
      }
    }
    return len;
  }

  void WebRtcConnection::writeSsrc(char* buf, int len, unsigned int ssrc) {
    rtpheader *head = (rtpheader*) buf;
    rtcpheader *chead = reinterpret_cast<rtcpheader*> (buf);
    //if it is RTCP we check it it is a compound packet
    if (chead->packettype == RTCP_Sender_PT || chead->packettype == RTCP_Receiver_PT || chead->packettype == RTCP_PS_Feedback_PT || chead->packettype == RTCP_RTP_Feedback_PT) {
        processRtcpHeaders(buf,len,ssrc);
    } else {
      head->ssrc=htonl(ssrc);
    }
  }

  void WebRtcConnection::onTransportData(char* buf, int len, Transport *transport) {
    boost::mutex::scoped_lock lock(writeMutex_);
    if (audioSink_ == NULL && videoSink_ == NULL && fbSink_==NULL)
      return;
    int length = len;
    rtcpheader *chead = reinterpret_cast<rtcpheader*> (buf);
    if (chead->packettype == RTCP_Receiver_PT || chead->packettype == RTCP_PS_Feedback_PT || chead->packettype == RTCP_RTP_Feedback_PT){
      if (fbSink_ != NULL) {
        fbSink_->deliverFeedback(buf,length);
      }
    } else {
      // RTP or RTCP Sender Report
      if (bundle_) {

        // Check incoming SSRC
        rtpheader *head = reinterpret_cast<rtpheader*> (buf);
        rtcpheader *chead = reinterpret_cast<rtcpheader*> (buf);
        unsigned int recvSSRC = ntohl(head->ssrc);

        if (chead->packettype == RTCP_Sender_PT) { //Sender Report
          ELOG_DEBUG ("RTP Sender Report %d length %d ", chead->packettype, ntohs(chead->length));
          recvSSRC = ntohl(chead->ssrc);
        }

        // Deliver data
        if (recvSSRC==this->getVideoSourceSSRC() || recvSSRC==this->getVideoSinkSSRC()) {
          videoSink_->deliverVideoData(buf, length);
        } else if (recvSSRC==this->getAudioSourceSSRC() || recvSSRC==this->getAudioSinkSSRC()) {
          audioSink_->deliverAudioData(buf, length);
        } else {
          ELOG_DEBUG("Unknown SSRC %u, localVideo %u, remoteVideo %u, ignoring", recvSSRC, this->getVideoSourceSSRC(), this->getVideoSinkSSRC());
        }
      } else if (transport->mediaType == AUDIO_TYPE) {
        if (audioSink_ != NULL) {
          rtpheader *head = (rtpheader*) buf;
          // Firefox does not send SSRC in SDP
          if (this->getAudioSourceSSRC() == 0) {
            ELOG_DEBUG("Audio Source SSRC is %u", ntohl(head->ssrc));
            this->setAudioSourceSSRC(ntohl(head->ssrc));
            this->updateState(TRANSPORT_READY, transport);
          }
          head->ssrc = htonl(this->getAudioSinkSSRC());
          audioSink_->deliverAudioData(buf, length);
        }
      } else if (transport->mediaType == VIDEO_TYPE) {
        if (videoSink_ != NULL) {
          rtpheader *head = (rtpheader*) buf;
          // Firefox does not send SSRC in SDP
          if (this->getVideoSourceSSRC() == 0) {
            ELOG_DEBUG("Video Source SSRC is %u", ntohl(head->ssrc));
            this->setVideoSourceSSRC(ntohl(head->ssrc));
            this->updateState(TRANSPORT_READY, transport);
          }

          head->ssrc = htonl(this->getVideoSinkSSRC());
          videoSink_->deliverVideoData(buf, length);
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
      videoTransport_->write((char*)rtcpPacket, pos);
    }

    return pos;
  }

  void WebRtcConnection::setWebRTCConnectionStateListener(
      WebRtcConnectionStateListener* listener) {
    this->connStateListener_ = listener;
  }

  void WebRtcConnection::updateState(TransportState state, Transport * transport) {
    boost::mutex::scoped_lock lock(updateStateMutex_);
    WebRTCState temp = INITIAL;
    if (audioTransport_ == NULL && videoTransport_ == NULL) {
      return;
    }

    if (state == TRANSPORT_STARTED &&
        (!remoteSdp_.hasAudio || (audioTransport_ != NULL && audioTransport_->getTransportState() == TRANSPORT_STARTED)) &&
        (!remoteSdp_.hasVideo || (videoTransport_ != NULL && videoTransport_->getTransportState() == TRANSPORT_STARTED))) {
      if (remoteSdp_.hasVideo) {
        videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
      }
      if (!bundle_ && remoteSdp_.hasAudio) {
        audioTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
      }
      temp = STARTED;
    }

    if (state == TRANSPORT_READY &&
        (!remoteSdp_.hasAudio || (audioTransport_ != NULL && audioTransport_->getTransportState() == TRANSPORT_READY)) &&
        (!remoteSdp_.hasVideo || (videoTransport_ != NULL && videoTransport_->getTransportState() == TRANSPORT_READY))) {
      if ((!remoteSdp_.hasAudio || this->getAudioSourceSSRC() != 0) &&
          (!remoteSdp_.hasVideo || this->getVideoSourceSSRC() != 0)) {
        temp = READY;
      }


    }

    if (transport != NULL && transport == videoTransport_ && bundle_) {
      if (state == TRANSPORT_STARTED) {
        videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
        temp = STARTED;
      }
      if (state == TRANSPORT_READY) {
        temp = READY;
      }
    }

    if (temp == READY && globalState_ != temp) {
      ELOG_INFO("Ready to send and receive media");
    }

    if (temp < 0) {
      return;
    }

    if (temp == globalState_ || (temp == STARTED && globalState_ == READY))
      return;

    globalState_ = temp;
    if (connStateListener_ != NULL)
      connStateListener_->connectionStateChanged(globalState_);
  }

  void WebRtcConnection::queueData(int comp, const char* buf, int length, Transport *transport) {
    receiveVideoMutex_.lock();
    if (sendQueue_.size() < 1000) {
      dataPacket p_;
      memset(p_.data, 0, length);
      memcpy(p_.data, buf, length);
      p_.comp = comp;
      if (transport->mediaType == VIDEO_TYPE) {
        p_.type = VIDEO_PACKET;
      } else {
        p_.type = AUDIO_PACKET;
      }

      p_.length = length;
      sendQueue_.push(p_);
    }
    receiveVideoMutex_.unlock();
  }

  WebRTCState WebRtcConnection::getCurrentState() {
    return globalState_;
  }

  void WebRtcConnection::processRtcpHeaders(char* buf, int len, unsigned int ssrc){
    char* movingBuf = buf;
    int rtcpLength = 0;
    int totalLength = 0;
    do{
      movingBuf+=rtcpLength;
      rtcpheader *chead= reinterpret_cast<rtcpheader*>(movingBuf);
      rtcpLength= (ntohs(chead->length)+1)*4;      
      totalLength+= rtcpLength;
      chead->ssrc=htonl(ssrc);
      if (chead->packettype == RTCP_PS_Feedback_PT){
        firheader *thefir = reinterpret_cast<firheader*>(movingBuf);
        if (thefir->fmt == 4){ // It is a FIR Packet, we generate it
          //ELOG_DEBUG("Feedback FIR packet, changed source %u sourcessrc to %u fmt %d", ssrc, sourcessrc, thefir->fmt);
          this->sendFirPacket();
        }
      }
    } while(totalLength<len);
  }

  void WebRtcConnection::sendLoop() {

    while (sending_ == true) {
      //    while (!boost::this_thread::interruption_requested()){
      receiveVideoMutex_.lock();
      if (sendQueue_.size() > 0) {
        if (sendQueue_.front().type == AUDIO_PACKET) {
          audioTransport_->writeOnNice(sendQueue_.front().comp, sendQueue_.front().data,
              sendQueue_.front().length);
        } else {
          videoTransport_->writeOnNice(sendQueue_.front().comp, sendQueue_.front().data,
              sendQueue_.front().length);
        }
        sendQueue_.pop();
        receiveVideoMutex_.unlock();
      } else {
        receiveVideoMutex_.unlock();
        usleep(1000);
      }
    }

    receiveVideoMutex_.unlock();
    ELOG_WARN("thread ended");
    }
}
/* namespace erizo */
