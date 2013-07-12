/*
 * WebRTCConnection.cpp
 */

#include <cstdio>

#include "WebRtcConnection.h"
#include "DtlsTransport.h"

#include "SdpInfo.h"

namespace erizo {

  WebRtcConnection::WebRtcConnection() {

    printf("WebRtcConnection constructor\n");
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

    sending = true;
    send_Thread_ = boost::thread(&WebRtcConnection::sendLoop, this);

    videoTransport_ = NULL;
    audioTransport_ = NULL;
    printf("WebRtcConnection constructor end\n");

  }

  WebRtcConnection::~WebRtcConnection() {
    
    this->close();
  }

  bool WebRtcConnection::init() {
    return true;
  }

  void WebRtcConnection::close() {
    if (videoTransport_ != NULL) {
      videoTransport_->close();
    }
    if (audioTransport_ != NULL) {
      audioTransport_->close();
    }
    if (sending != false) {
      sending = false;
      send_Thread_.join();
    }
  }

  void WebRtcConnection::closeSink(){
    this->close();
  }

  void WebRtcConnection::closeSource(){
    this->close();
  }

  bool WebRtcConnection::setRemoteSdp(const std::string &sdp) {
    printf("Set Remote SDP\n %s", sdp.c_str());
    remoteSdp_.initWithSdp(sdp);
    std::vector<CryptoInfo> crypto_remote = remoteSdp_.getCryptoInfos();
    video_ = (remoteSdp_.videoSsrc==0?false:true);
    audio_ = (remoteSdp_.audioSsrc==0?false:true);

    CryptoInfo cryptLocal_video;
    CryptoInfo cryptLocal_audio;
    CryptoInfo cryptRemote_video;
    CryptoInfo cryptRemote_audio;

    bundle_ = remoteSdp_.isBundle;
    printf("Is bundle? %d %d \n", bundle_, true);
    std::vector<RtpMap> payloadRemote = remoteSdp_.getPayloadInfos();
    localSdp_.getPayloadInfos() = remoteSdp_.getPayloadInfos();
    localSdp_.isBundle = bundle_;
    localSdp_.isRtcpMux = remoteSdp_.isRtcpMux;

    printf("Video %d videossrc %u Audio %d audio ssrc %u Bundle %d \n", video_, remoteSdp_.videoSsrc, audio_, remoteSdp_.audioSsrc,  bundle_);

    if (!remoteSdp_.isFingerprint && remoteSdp_.profile == SAVPF){
        // TODO Add support for SDES
        CryptoInfo crytpv;
        crytpv.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
        crytpv.mediaType = VIDEO_TYPE;
        std::string keyv = SrtpChannel::generateBase64Key();
        crytpv.keyParams = keyv;
        crytpv.tag = 1;
        localSdp_.addCrypto(crytpv);
        //    audioSrtp_ = new SrtpChannel();
        CryptoInfo crytpa;
        crytpa.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
        crytpa.mediaType = AUDIO_TYPE;
        crytpa.tag = 1;
        //    std::string keya = SrtpChannel::generateBase64Key();
        crytpa.keyParams = keyv;
        localSdp_.addCrypto(crytpa);
    }
    printf("Setting SSRC to localSdp %u\n", this->getVideoSinkSSRC());
    localSdp_.videoSsrc = this->getVideoSinkSSRC();
    localSdp_.audioSsrc = this->getAudioSinkSSRC();

    std::vector<CryptoInfo> crypto_local = localSdp_.getCryptoInfos();

    for (unsigned int it = 0; it < crypto_remote.size(); it++) {
      CryptoInfo cryptemp = crypto_remote[it];
      if (cryptemp.mediaType == VIDEO_TYPE
          && !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
        video_ = true;
        cryptRemote_video = cryptemp;
      } else if (cryptemp.mediaType == AUDIO_TYPE
          && !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
        audio_ = true;
        cryptRemote_audio = cryptemp;
      }
    }
    for (unsigned int it = 0; it < crypto_local.size(); it++) {
      CryptoInfo cryptemp = crypto_local[it];
      if (cryptemp.mediaType == VIDEO_TYPE
          && !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
        cryptLocal_video = cryptemp;
      } else if (cryptemp.mediaType == AUDIO_TYPE
          && !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
        cryptLocal_audio = cryptemp;
      }
    }

    videoTransport_ = new DtlsTransport(VIDEO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, this);
    audioTransport_ = new DtlsTransport(AUDIO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, this);

    this->setVideoSourceSSRC(remoteSdp_.videoSsrc);
    this->setAudioSourceSSRC(remoteSdp_.audioSsrc);

    return true;
  }

  std::string WebRtcConnection::getLocalSdp() {
    printf("Getting SDP\n");
    if (videoTransport_ != NULL) {
        videoTransport_->processLocalSdp(&localSdp_);
    }
    printf("Video SDP done.\n");
    if (!bundle_ && audioTransport_ != NULL) {
        audioTransport_->processLocalSdp(&localSdp_);
    }
    printf("Audio SDP done.\n");
    localSdp_.profile = remoteSdp_.profile;
    return localSdp_.getSdp();
  }

  int WebRtcConnection::deliverAudioData(char* buf, int len) {
    boost::mutex::scoped_lock lock(receiveAudioMutex_);
    if (bundle_){
      printf("Sending audio to audio channel Bad\n");
      videoTransport_->write(buf, len, this->getVideoSinkSSRC());
    } else {
      audioTransport_->write(buf, len, this->getAudioSinkSSRC());
    }
    return len;
  }

  int WebRtcConnection::deliverVideoData(char* buf, int len) {
    boost::mutex::scoped_lock lock(receiveAudioMutex_);
    //printf("Sending video\n");
    rtpheader *head = (rtpheader*) buf;
        if (head->payloadtype == 0) {
          printf("Sending audio on video!!!!!!! %d\n", head->payloadtype);  
        }
    videoTransport_->write(buf, len, this->getVideoSinkSSRC());
    return len;
  }

  int WebRtcConnection::deliverFeedback(char* buf, int len){
    this->deliverVideoData(buf,len);
    return len;
  }



  void WebRtcConnection::onTransportData(char* buf, int len, bool rtcp, unsigned int recvSSRC, Transport *transport) {
    boost::mutex::scoped_lock lock(writeMutex_);
    if (audioSink_ == NULL && videoSink_ == NULL && fbSink_==NULL)
      return;
    int length = len;

    if (rtcp) {
      if (fbSink_ != NULL) {
        fbSink_->deliverFeedback(buf,length);
      }
    } else if (bundle_) {
      if (recvSSRC==this->getVideoSourceSSRC() || recvSSRC==this->getVideoSinkSSRC()) {
        videoSink_->deliverVideoData(buf, length);
      } else if (recvSSRC==this->getAudioSourceSSRC() || recvSSRC==this->getAudioSinkSSRC()) {
        videoSink_->deliverAudioData(buf, length);
      } else {
        printf("Unknown SSRC %u, localVideo %u, remoteVideo %u, ignoring\n", recvSSRC, this->getVideoSourceSSRC(), this->getVideoSinkSSRC());
      }
    } else if (transport->mediaType == AUDIO_TYPE) {
      if (audioSink_ != NULL) {
        rtpheader *head = (rtpheader*) buf;
        head->ssrc = htonl(this->getAudioSinkSSRC());

        
        audioSink_->deliverAudioData(buf, length);
      }
    } else if (transport->mediaType == VIDEO_TYPE) {
      if (videoSink_ != NULL) {
        rtpheader *head = (rtpheader*) buf;
        if (head->payloadtype == 0) {
          printf("Received audio on video!!!!!!! %d %p\n", head->payloadtype, transport);  
        }
        head->ssrc = htonl(this->getVideoSinkSSRC());
        videoSink_->deliverVideoData(buf, length);
      }
    }
  }

  int WebRtcConnection::sendFirPacket() {
    printf("SendingFIR\n");
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

    videoTransport_->write((char*)rtcpPacket, pos, this->getVideoSinkSSRC());

    return pos;
  }

  void WebRtcConnection::setWebRTCConnectionStateListener(
      WebRtcConnectionStateListener* listener) {
    this->connStateListener_ = listener;
  }

  void WebRtcConnection::updateState(TransportState state, Transport * transport) {
    boost::mutex::scoped_lock lock(updateStateMutex_);
    WebRTCState temp;
    if (audioTransport_ == NULL && videoTransport_ == NULL) {
      return;
    }


    if (state == TRANSPORT_STARTED &&
      audioTransport_ != NULL && audioTransport_->getTransportState() == TRANSPORT_STARTED &&
      videoTransport_ != NULL && videoTransport_->getTransportState() == TRANSPORT_STARTED) {
      videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
      if (!bundle_) {
        audioTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
      }
      temp = STARTED;
    }

    if (transport == videoTransport_ && bundle_) {
      if (state == TRANSPORT_READY) {
            printf("Ready to send and receive media!!\n");
            temp = READY;
        }
    }

    //printf("State %d, Audio %d, Video %d, Total: %d \n", state, audioTransport_->getTransportState(), videoTransport_->getTransportState(), temp);

    if (temp < 0) {
      return;
    }

    if (temp == globalState_ || (temp == STARTED && globalState_ == READY))
      return;

    printf("New WebRTC state: %d, %d %d \n", state, bundle_, temp);
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

  void WebRtcConnection::sendLoop() {

    while (sending == true) {
      receiveVideoMutex_.lock();
      if (sendQueue_.size() > 0) {
        if (sendQueue_.front().type == AUDIO_PACKET && audioTransport_!=NULL) {
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
  }
}
/* namespace erizo */
