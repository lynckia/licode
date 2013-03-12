/*
 * WebRTCConnection.cpp
 */

#include <cstdio>

#include "WebRtcConnection.h"
#include "NiceConnection.h"

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
    audioNice_ = NULL;
    videoNice_ = NULL;
    audioSrtp_ = NULL;
    videoSrtp_ = NULL;
    fbSink_ = NULL;
    sourcefbSink_ = this;
    sinkfbSource_ = this;
    
    globalIceState_ = INITIAL;
    connStateListener_ = NULL;

    sending = true;
    send_Thread_ = boost::thread(&WebRtcConnection::sendLoop, this);

    videoNice_ = new NiceConnection(VIDEO_TYPE, "");
    videoNice_->setWebRtcConnection(this);

    audioNice_ = new NiceConnection(AUDIO_TYPE, "");
    audioNice_->setWebRtcConnection(this);

  }

  WebRtcConnection::~WebRtcConnection() {

    this->close();
  }

  bool WebRtcConnection::init() {
    videoNice_->start();
    audioNice_->start();
    return true;
  }

  void WebRtcConnection::close() {
    if (sending != false) {
      sending = false;
      send_Thread_.join();
    }
    if (audio_) {
      if (audioNice_ != NULL) {
        audioNice_->close();
        audioNice_->join();
        delete audioNice_;
      }
      if (audioSrtp_ != NULL)
        delete audioSrtp_;
    }
    if (video_) {
      if (videoNice_ != NULL) {
        videoNice_->close();
        videoNice_->join();
        delete videoNice_;
      }
      if (videoSrtp_ != NULL)
        delete videoSrtp_;
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
    //    video_ = (remoteSdp_.videoSsrc==0?false:true);
    //    audio_ = (remoteSdp_.audioSsrc==0?false:true);
    video_=1;
    audio_=1;

    CryptoInfo cryptLocal_video;
    CryptoInfo cryptLocal_audio;
    CryptoInfo cryptRemote_video;
    CryptoInfo cryptRemote_audio;

    bundle_ = remoteSdp_.isBundle;
    std::vector<RtpMap> payloadRemote = remoteSdp_.getPayloadInfos();
    localSdp_.getPayloadInfos() = remoteSdp_.getPayloadInfos();
    localSdp_.isBundle = bundle_;
    localSdp_.isRtcpMux = remoteSdp_.isRtcpMux;

    printf("Video %d videossrc %u Audio %d audio ssrc %u Bundle %d ", video_, remoteSdp_.videoSsrc, audio_, remoteSdp_.audioSsrc,  bundle_);

    if (!bundle_) {
      printf("NOT BUNDLE\n");
      if (video_) {
        //        if (remoteSdp_.getCryptoInfos().size()!=0){
        if (remoteSdp_.profile==SAVPF){
          videoSrtp_ = new SrtpChannel();
          CryptoInfo crytp;
          crytp.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
          crytp.mediaType = VIDEO_TYPE;
          std::string key = SrtpChannel::generateBase64Key();

          crytp.keyParams = key;
          crytp.tag = 0;
          localSdp_.addCrypto(crytp);
        }
        //        }
        localSdp_.videoSsrc = this->getVideoSinkSSRC();
      } else {
        videoNice_->close();
        delete (videoNice_);
      }

      if (audio_) {
        if (remoteSdp_.profile==SAVPF){
          audioSrtp_ = new SrtpChannel();
          CryptoInfo crytp;
          crytp.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
          crytp.mediaType = AUDIO_TYPE;
          crytp.tag = 1;
          std::string key = SrtpChannel::generateBase64Key();
          crytp.keyParams = key;
          localSdp_.addCrypto(crytp);
        }
        localSdp_.audioSsrc = this->getAudioSinkSSRC();
      } else {
        audioNice_->close();
        audioNice_->join();
        delete(audioNice_);
      }

    } else {
      audioNice_->close();
      delete(audioNice_);
      audioNice_=NULL;
      if(remoteSdp_.profile == SAVPF){
        videoSrtp_ = new SrtpChannel();
        CryptoInfo crytpv;
        crytpv.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
        crytpv.mediaType = VIDEO_TYPE;
        std::string keyv = SrtpChannel::generateBase64Key();
        crytpv.keyParams = keyv;
        crytpv.tag = 1;
        localSdp_.addCrypto(crytpv);
        //		audioSrtp_ = new SrtpChannel();
        CryptoInfo crytpa;
        crytpa.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
        crytpa.mediaType = AUDIO_TYPE;
        crytpa.tag = 1;
        //		std::string keya = SrtpChannel::generateBase64Key();
        crytpa.keyParams = keyv;
        localSdp_.addCrypto(crytpa);
      }

      localSdp_.videoSsrc = this->getVideoSinkSSRC();
      localSdp_.audioSsrc = this->getAudioSinkSSRC();

    }

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
    if (!bundle_) {
      if (video_) {
        videoNice_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
        if(videoSrtp_!=NULL){
          videoSrtp_->setRtpParams((char*) cryptLocal_video.keyParams.c_str(),
              (char*) cryptRemote_video.keyParams.c_str());
        }

      }
      if (audio_) {
        audioNice_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
        if (audioSrtp_!=NULL){
          audioSrtp_->setRtpParams((char*) cryptLocal_audio.keyParams.c_str(),
              (char*) cryptRemote_audio.keyParams.c_str());
        }
      }
    } else {
      videoNice_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
      this->setVideoSourceSSRC(remoteSdp_.videoSsrc);
      this->setAudioSourceSSRC(remoteSdp_.audioSsrc);
      videoSrtp_->setRtpParams((char*) cryptLocal_video.keyParams.c_str(),
          (char*) cryptRemote_video.keyParams.c_str());
      videoSrtp_->setRtcpParams((char*) cryptLocal_video.keyParams.c_str(),
          (char*) cryptRemote_video.keyParams.c_str());
      //		audioSrtp_->setRtpParams((char*)cryptLocal_audio.keyParams.c_str(), (char*)cryptRemote_audio.keyParams.c_str());

    }
    return true;
  }

  std::string WebRtcConnection::getLocalSdp() {
    std::vector<CandidateInfo> *cands;
    if (bundle_) {
      if (videoNice_->iceState > CANDIDATES_GATHERED) {
        cands = videoNice_->localCandidates;
        for (unsigned int it = 0; it < cands->size(); it++) {
          CandidateInfo cand = cands->at(it);
          cand.isBundle = bundle_;
          localSdp_.addCandidate(cand);
          cand.mediaType = AUDIO_TYPE;
          localSdp_.addCandidate(cand);
        }
      } else {
        printf("WARNING getting local sdp before it is ready!\n");
      }
    } else {
      if (video_ && videoNice_->iceState > CANDIDATES_GATHERED) {
        cands = videoNice_->localCandidates;
        for (unsigned int it = 0; it < cands->size(); it++) {
          CandidateInfo cand = cands->at(it);
          localSdp_.addCandidate(cand);
        }
      }
      if (audio_ && audioNice_->iceState > CANDIDATES_GATHERED) {
        cands = audioNice_->localCandidates;
        for (unsigned int it = 0; it < cands->size(); it++) {
          CandidateInfo cand = cands->at(it);
          localSdp_.addCandidate(cand);
        }
      }
    }
    localSdp_.profile = remoteSdp_.profile;
    return localSdp_.getSdp();
  }

  int WebRtcConnection::deliverAudioData(char* buf, int len) {
    boost::mutex::scoped_lock lock(receiveAudioMutex_);
    int res = -1;
    int length = len;

    if (bundle_){

      if (videoSrtp_ && videoNice_->iceState == READY) {

        rtcpheader *chead = reinterpret_cast<rtcpheader*> (buf);
        if (chead->packettype == 200 || chead->packettype == 201) {
          chead->ssrc=htonl(this->getVideoSinkSSRC());
          if(videoSrtp_->protectRtcp(buf, &length)<0)
            return length;
        }else{
          rtpheader* inHead = reinterpret_cast<rtpheader*> (buf);
          inHead->ssrc = htonl(this->getAudioSinkSSRC());
          if(videoSrtp_->protectRtp(buf, &length)<0)
            return length;
        }
      }
      if (length <= 10)
        return length;
      if (videoNice_->iceState == READY) {
        receiveVideoMutex_.lock();
        if (sendQueue_.size() < 1000) {
          packet p_;
          memset(p_.data, 0, length);
          memcpy(p_.data, buf, length);

          p_.length = length;
          sendQueue_.push(p_);
        }
        receiveVideoMutex_.unlock();
      }
    }else{

      rtpheader* inHead = reinterpret_cast<rtpheader*> (buf);
      inHead->ssrc = htonl(this->getAudioSinkSSRC());
      if (audioSrtp_) {
        if(audioSrtp_->protectRtp(buf, &length)<0)
          return length;
      }
      if (len <= 0)
        return length;
      if (audioNice_) {
        res = audioNice_->sendData(buf, length);
      }
    }
    return res;
  }

  int WebRtcConnection::deliverVideoData(char* buf, int len) {

    int res = -1;
    int length = len;

    rtcpheader *chead = reinterpret_cast<rtcpheader*> (buf);
    if (chead->packettype == 200 || chead->packettype == 201) {
      chead->ssrc=htonl(this->getVideoSinkSSRC());
      if (videoSrtp_ && videoNice_->iceState == READY) {
        if(videoSrtp_->protectRtcp(buf, &length)<0)
          return length;
      }
    }
    else{
      rtpheader* inHead = reinterpret_cast<rtpheader*> (buf);
      inHead->ssrc = htonl(this->getVideoSinkSSRC());
      if (videoSrtp_ && videoNice_->iceState == READY) {
        if(videoSrtp_->protectRtp(buf, &length)<0)
          return length;
      }
    }
    if (length <= 10)
      return length;
    if (videoNice_->iceState == READY) {
      receiveVideoMutex_.lock();
      if (sendQueue_.size() < 1000) {
        packet p_;
        memset(p_.data, 0, length);
        memcpy(p_.data, buf, length);

        p_.length = length;
        sendQueue_.push(p_);
      }
      receiveVideoMutex_.unlock();
    }
    return res;
  }

  int WebRtcConnection::deliverFeedback(char* buf, int len){
    printf("receiving Feedback\n");
    this->deliverVideoData(buf,len);
    return 0;
  }

  int WebRtcConnection::receiveNiceData(char* buf, int len,
      NiceConnection* nice) {
    boost::mutex::scoped_lock lock(writeMutex_);
    if (audioSink_ == NULL && videoSink_ == NULL)
      return 0;
    int length = len;

    unsigned int recvSSRC = 0;
    if (bundle_) {
      bool isrtcp = false;
      if (videoSrtp_){
        rtcpheader *chead = reinterpret_cast<rtcpheader*> (buf);
        if (fbSink_!=NULL && chead->packettype == 200) {
          if (videoSrtp_->unprotectRtcp(buf, &length)<0)
            return length;
          recvSSRC = ntohl(chead->ssrc);
          isrtcp = true;
          return length;
        } else if (fbSink_!=NULL && chead->packettype == 201){
          if(videoSrtp_->unprotectRtcp(buf, &length)<0)
            return length;
          recvSSRC = ntohl(chead->ssrcsource);
        } else {
          if(videoSrtp_->unprotectRtp(buf, &length)<0)
            return length;
        }
      }

      if (length <= 0)
        return length;
      if (recvSSRC==0){
        rtpheader* inHead = reinterpret_cast<rtpheader*> (buf);
        recvSSRC= ntohl(inHead->ssrc);
      }

      if (recvSSRC==this->getVideoSourceSSRC() || recvSSRC==this->getVideoSinkSSRC()) {
        if (isrtcp){
          fbSink_->deliverFeedback(buf,len);
        }else{
          videoSink_->deliverVideoData(buf, length);
        }
      } else if (recvSSRC==this->getAudioSourceSSRC() || recvSSRC==this->getAudioSinkSSRC()) {
        videoSink_->deliverAudioData(buf, length);
      } else {
        printf("Unknown SSRC %u, localVideo %u, remoteVideo %u, ignoring\n", recvSSRC, this->getVideoSourceSSRC(), this->getVideoSinkSSRC());
      }

      return length;
    }

    if (nice->mediaType == AUDIO_TYPE) {
      if (audioSink_ != NULL) {
        if (audioSrtp_) {
          if(audioSrtp_->unprotectRtp(buf, &length)<0)
            return length;
        }
        if (length <= 0)
          return length;
        rtpheader *head = (rtpheader*) buf;
        head->ssrc = htonl(this->getAudioSinkSSRC());
        audioSink_->deliverAudioData(buf, length);
        return length;
      }
    } else if (nice->mediaType == VIDEO_TYPE) {
      if (videoSink_ != NULL) {
        if (videoSrtp_) {
          if(videoSrtp_->unprotectRtp(buf, &length)<0)
            return length;
        }
        if (length <= 0)
          return length;
        rtpheader *head = (rtpheader*) buf;
        head->ssrc = htonl(this->getVideoSinkSSRC());
        videoSink_->deliverVideoData(buf, length);
        return length;
      }
    }
    return -1;
  }

  int WebRtcConnection::sendFirPacket() {
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
    if (videoNice_ != NULL && videoNice_->iceState == READY) {
      if (videoSrtp_!=NULL)
        if(videoSrtp_->protectRtcp((char*) rtcpPacket, &pos)<0)
          return pos;
      videoNice_->sendData((char*) rtcpPacket, pos);
    }
    return pos;
  }

  void WebRtcConnection::setWebRTCConnectionStateListener(
      WebRtcConnectionStateListener* listener) {
    this->connStateListener_ = listener;
  }

  void WebRtcConnection::updateState(IceState newState,
      NiceConnection* niceConn) {
    boost::mutex::scoped_lock lock(updateStateMutex_);
    IceState temp;
    if (niceConn == videoNice_&& bundle_){
      temp = newState;
    }else{
      if (audioNice_ == NULL || videoNice_ == NULL){
        temp = newState;
      }else{
        temp = audioNice_->iceState<=videoNice_->iceState? audioNice_->iceState:videoNice_->iceState;
      }

    }
    if (temp == globalIceState_)
      return;
    globalIceState_ = temp;
    if (connStateListener_ != NULL)
      connStateListener_->connectionStateChanged(globalIceState_);
  }

  IceState WebRtcConnection::getCurrentState() {
    return globalIceState_;
  }

  void WebRtcConnection::sendLoop() {

    while (sending == true) {
      receiveVideoMutex_.lock();
      if (sendQueue_.size() > 0) {
        videoNice_->sendData(sendQueue_.front().data,
            sendQueue_.front().length);
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
