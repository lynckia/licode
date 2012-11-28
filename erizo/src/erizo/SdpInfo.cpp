/*
 * SDPProcessor.cpp
 */

#include <sstream>
#include <stdio.h>
#include <cstdlib>
#include <cstring>

#include "SdpInfo.h"

using std::endl;
namespace erizo {

  static const char *cand = "a=candidate:";
  static const char *crypto = "a=crypto:";
  static const char *group = "a=group:";
  static const char *video = "m=video";
  static const char *audio = "m=audio";
  static const char *ice_user = "a=ice-ufrag";
  static const char *ice_pass = "a=ice-pwd";
  static const char *ssrctag = "a=ssrc";
  static const char *savpf = "SAVPF";
  static const char *rtpmap = "a=rtpmap:";
  static const char *rtcpmux = "a=rtcp-mux";

  SdpInfo::SdpInfo() {
    isBundle = false;
    isRtcpMux = false;
    profile = AVPF;
    audioSsrc = 0;
    videoSsrc = 0;
  }

  SdpInfo::~SdpInfo() {
  }

  bool SdpInfo::initWithSdp(const std::string& sdp) {
    processSdp(sdp);
    return true;
  }
  void SdpInfo::addCandidate(const CandidateInfo& info) {
    candidateVector_.push_back(info);

  }

  void SdpInfo::addCrypto(const CryptoInfo& info) {
    cryptoVector_.push_back(info);
  }

  std::string SdpInfo::getSdp() {

    std::ostringstream sdp;
    sdp << "v=0\n" << "o=- 0 0 IN IP4 127.0.0.1\n" << "s=\n" << "t=0 0\n";

    if (isBundle) {
      sdp << "a=group:BUNDLE audio video\n";
    }
    //candidates audio
    bool printedAudio = false, printedVideo = false;
    for (unsigned int it = 0; it < candidateVector_.size(); it++) {
      const CandidateInfo& cand = candidateVector_[it];
      std::string hostType_str;
      switch (cand.hostType) {
        case HOST:
          hostType_str = "host";
          break;
        case SRLFX:
          hostType_str = "srflx";
          break;
        case PRFLX:
          hostType_str = "prflx";
          break;
        case RELAY:
          hostType_str = "relay";
          break;
        default:
          hostType_str = "host";
          break;
      }
      if (cand.mediaType == AUDIO_TYPE) {
        if (!printedAudio) {
          sdp << "m=audio " << cand.hostPort
            << " RTP/" << (profile==SAVPF?"SAVPF ":"AVPF ");// << "103 104 0 8 106 105 13 126\n"
          for (unsigned int it =0; it<payloadVector_.size(); it++){
            const RtpMap& payload_info = payloadVector_[it];
            if (payload_info.mediaType == AUDIO_TYPE)
              sdp << payload_info.payloadType <<" ";

          }
          sdp << "\n"
            << "c=IN IP4 " << cand.hostAddress
            << endl << "a=rtcp:" << candidateVector_[0].hostPort
            << " IN IP4 " << cand.hostAddress
            << endl;
          printedAudio = true;
        }

        sdp << "a=candidate:" << cand.foundation << " " << cand.componentId
          << " " << cand.netProtocol << " " << cand.priority << " "
          << cand.hostAddress << " " << cand.hostPort << " typ "
          << hostType_str << " generation 0" << endl;

        iceUsername_ = cand.username;
        icePassword_ = cand.password;
      }
    }
    //crypto audio
    if (printedAudio) {
      sdp << "a=ice-ufrag:" << iceUsername_ << endl;
      sdp << "a=ice-pwd:" << icePassword_ << endl;
      sdp << "a=sendrecv" << endl;
      sdp << "a=mid:audio\n";
      if (isRtcpMux)
        sdp << "a=rtcp-mux\n";
      for (unsigned int it = 0; it < cryptoVector_.size(); it++) {
        const CryptoInfo& cryp_info = cryptoVector_[it];
        if (cryp_info.mediaType == AUDIO_TYPE) {
          sdp << "a=crypto:" << cryp_info.tag << " "
            << cryp_info.cipherSuite << " " << "inline:"
            << cryp_info.keyParams << endl;
        }
      }

      for (unsigned int it = 0; it < payloadVector_.size(); it++) {
        const RtpMap& rtp = payloadVector_[it];
        if (rtp.mediaType==AUDIO_TYPE)
          sdp << "a=rtpmap:"<<rtp.payloadType << " " << rtp.encodingName << "/"
            << rtp.clockRate <<"\n";
      }
      sdp << "a=ssrc:" << audioSsrc << " cname:o/i14u9pJrxRKAsu\na=ssrc:"
        << audioSsrc
        << " mslabel:048f838f-2dd1-4a98-ab9e-8eb5f00abab8\na=ssrc:"
        << audioSsrc << " label:iSight integrada\n";

    }

    for (unsigned int it = 0; it < candidateVector_.size(); it++) {
      const CandidateInfo& cand = candidateVector_[it];
      std::string hostType_str;
      switch (cand.hostType) {
        case HOST:
          hostType_str = "host";
          break;
        case SRLFX:
          hostType_str = "srflx";
          break;
        case PRFLX:
          hostType_str = "prflx";
          break;
        case RELAY:
          hostType_str = "relay";
          break;
        default:
          hostType_str = "host";
          break;
      }
      if (cand.mediaType == VIDEO_TYPE) {
        if (!printedVideo) {
          sdp << "m=video " << cand.hostPort << " RTP/" << (profile==SAVPF?"SAVPF ":"AVPF "); //<<  "100 101 102 103\n"
          for (unsigned int it =0; it<payloadVector_.size(); it++){
            const RtpMap& payload_info = payloadVector_[it];
            if (payload_info.mediaType == VIDEO_TYPE)
              sdp << payload_info.payloadType <<" ";
          }

          sdp << "\n" << "c=IN IP4 " << cand.hostAddress
            << endl << "a=rtcp:" << candidateVector_[0].hostPort
            << " IN IP4 " << cand.hostAddress
            << endl;
          printedVideo = true;
        }

        sdp << "a=candidate:" << cand.foundation << " " << cand.componentId
          << " " << cand.netProtocol << " " << cand.priority << " "
          << cand.hostAddress << " " << cand.hostPort << " typ "
          << hostType_str << " generation 0" << endl;

        iceUsername_ = cand.username;
        icePassword_ = cand.password;
      }
    }
    //crypto audio
    if (printedVideo) {
      sdp << "a=ice-ufrag:" << iceUsername_ << endl;
      sdp << "a=ice-pwd:" << icePassword_ << endl;
      sdp << "a=sendrecv" << endl;
      sdp << "a=mid:video\n";
      if (isRtcpMux) 
        sdp << "a=rtcp-mux\n";
      for (unsigned int it = 0; it < cryptoVector_.size(); it++) {
        const CryptoInfo& cryp_info = cryptoVector_[it];
        if (cryp_info.mediaType == VIDEO_TYPE) {
          sdp << "a=crypto:" << cryp_info.tag << " "
            << cryp_info.cipherSuite << " " << "inline:"
            << cryp_info.keyParams << endl;
        }
      }

      for (unsigned int it = 0; it < payloadVector_.size(); it++) {
        const RtpMap& rtp = payloadVector_[it];
        if (rtp.mediaType==VIDEO_TYPE)
          sdp << "a=rtpmap:"<<rtp.payloadType << " " << rtp.encodingName << "/"
            << rtp.clockRate <<"\n";
      }

      sdp << "a=ssrc:" << videoSsrc << " cname:o/i14u9pJrxRKAsu\na=ssrc:"
        << videoSsrc
        << " mslabel:048f838f-2dd1-4a98-ab9e-8eb5f00abab8\na=ssrc:"
        << videoSsrc << " label:iSight integrada\n";
    }
    printf("sdp local \n %s\n",sdp.str().c_str());
    return sdp.str();
  }

  bool SdpInfo::processSdp(const std::string& sdp) {

    std::string strLine;
    std::istringstream iss(sdp);
    char* line = (char*) malloc(1000);
    char** pieces = (char**) malloc(10000);
    char** cryptopiece = (char**) malloc(5000);

    MediaType mtype = OTHER;

    while (std::getline(iss, strLine)) {
      const char* theline = strLine.c_str();
      sprintf(line, "%s\n", theline);
      char* isVideo = strstr(line, video);
      char* isAudio = strstr(line, audio);
      char* isGroup = strstr(line, group);
      char* isCand = strstr(line, cand);
      char* isCrypt = strstr(line, crypto);
      char* isUser = strstr(line, ice_user);
      char* isPass = strstr(line, ice_pass);
      char* isSsrc = strstr(line, ssrctag);
      char* isSAVPF = strstr(line, savpf);
      char* isRtpmap = strstr(line,rtpmap);
      char* isRtcpMuxchar = strstr(line,rtcpmux);
      if (isRtcpMuxchar){
        isRtcpMux = true;
      }
      if (isSAVPF){
        profile = SAVPF;
        printf("PROFILE %s (1 SAVPF)\n", isSAVPF);
      }
      if (isGroup) {
        isBundle = true;
      }
      if (isVideo) {
        mtype = VIDEO_TYPE;
      }
      if (isAudio) {
        mtype = AUDIO_TYPE;
      }
      if (isCand != NULL) {
        char *pch;
        pch = strtok(line, " :");
        pieces[0] = pch;
        int i = 0;
        while (pch != NULL) {
          pch = strtok(NULL, " :");
          pieces[i++] = pch;
        }

        processCandidate(pieces, i - 1, mtype);
      }
      if (isCrypt) {
        //	printf("crypt %s\n", isCrypt );
        CryptoInfo crypinfo;
        char *pch;
        pch = strtok(line, " :");
        cryptopiece[0] = pch;
        int i = 0;
        while (pch != NULL) {
          pch = strtok(NULL, " :");
          //				printf("cryptopiece %i es %s\n", i, pch);
          cryptopiece[i++] = pch;
        }

        crypinfo.cipherSuite = std::string(cryptopiece[1]);
        crypinfo.keyParams = std::string(cryptopiece[3]);
        crypinfo.mediaType = mtype;
        cryptoVector_.push_back(crypinfo);
        //			sprintf(key, "%s",cryptopiece[3]);
        //				keys = g_slist_append(keys,key);
      }
      if (isUser) {
        char* pch;
        pch = strtok(line, " : \n");
        pch = strtok(NULL, " : \n");
        iceUsername_ = std::string(pch);

      }
      if (isPass) {
        char* pch;
        pch = strtok(line, " : \n");
        pch = strtok(NULL, ": \n");
        icePassword_ = std::string(pch);
      }
      if (isSsrc) {
        char* pch;
        pch = strtok(line, " : \n");
        pch = strtok(NULL, ": \n");
        if (mtype == VIDEO_TYPE) {
          videoSsrc = strtoul(pch, NULL, 10);
        } else if (mtype == AUDIO_TYPE) {
          audioSsrc = strtoul(pch, NULL, 10);
        }
      }
      // a=rtpmap:PT codec_name/clock_rate
      if(isRtpmap){
        RtpMap theMap; 
        char* pch;
        pch = strtok(line, " : / \n");
        pch = strtok(NULL, " : / \n");
        unsigned int PT = strtoul(pch, NULL, 10);
        pch = strtok(NULL, " : / \n");
        std::string codecname(pch);
        pch = strtok(NULL, " : / \n");
        unsigned int clock = strtoul(pch, NULL, 10);
        theMap.payloadType = PT;
        theMap.encodingName = codecname;
        theMap.clockRate = clock;
        theMap.mediaType = mtype;
        payloadVector_.push_back(theMap);
      }

    }

    free(line);
    free(pieces);
    free(cryptopiece);

    for (unsigned int i = 0; i < candidateVector_.size(); i++) {
      CandidateInfo& c = candidateVector_[i];
      c.username = iceUsername_;
      c.password = icePassword_;
      c.isBundle = isBundle;
    }

    return true;
  }

  std::vector<CandidateInfo>& SdpInfo::getCandidateInfos() {
    return candidateVector_;
  }

  std::vector<CryptoInfo>& SdpInfo::getCryptoInfos() {
    return cryptoVector_;
  }

  std::vector<RtpMap>& SdpInfo::getPayloadInfos(){
    return payloadVector_;
  }

  bool SdpInfo::processCandidate(char** pieces, int size, MediaType mediaType) {

    CandidateInfo cand;
    const char* types_str[10] = { "host", "srflx", "prflx", "relay" };
    cand.mediaType = mediaType;
    cand.foundation = pieces[0];
    cand.componentId = (unsigned int) strtoul(pieces[1], NULL, 10);

    cand.netProtocol = pieces[2];
    // libnice does not support tcp candidates, we ignore them
    if (cand.netProtocol.compare("udp")) {
      return false;
    }
    //	a=candidate:0 1 udp 2130706432 138.4.4.143 52314 typ host  generation 0
    //		        0 1 2    3            4          5     6  7    8          9
    cand.priority = (unsigned int) strtoul(pieces[3], NULL, 10);
    cand.hostAddress = std::string(pieces[4]);
    cand.hostPort = (unsigned int) strtoul(pieces[5], NULL, 10);
    if (strcmp(pieces[6], "typ")) {
      return false;
    }
    unsigned int type = 1111;
    int p;
    for (p = 0; p < 4; p++) {
      if (!strcmp(pieces[7], types_str[p])) {
        type = p;
      }
    }
    switch (type) {
      case 0:
        cand.hostType = HOST;
        break;
      case 1:
        cand.hostType = SRLFX;
        break;
      case 2:
        cand.hostType = PRFLX;
        break;
      case 3:
        cand.hostType = RELAY;
        break;
      default:
        cand.hostType = HOST;
        break;
    }

    if (type == 3) {
      cand.relayAddress = std::string(pieces[8]);
      cand.relayPort = (unsigned int) strtoul(pieces[9], NULL, 10);
    }
    candidateVector_.push_back(cand);
    return true;
  }

}/* namespace erizo */

