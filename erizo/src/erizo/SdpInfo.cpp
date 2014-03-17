/*
 * SDPProcessor.cpp
 */

#include <sstream>
#include <stdio.h>
#include <cstdlib>
#include <cstring>

#include "rtp/RtpHeaders.h"
#include "SdpInfo.h"

using std::endl;
namespace erizo {
  DEFINE_LOGGER(SdpInfo, "SdpInfo");

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
  static const char *fp = "a=fingerprint";

  SdpInfo::SdpInfo() {
    isBundle = false;
    isRtcpMux = false;
    isFingerprint = false;
    hasAudio = false;
    hasVideo = false;
    profile = AVPF;
    audioSsrc = 0;
    videoSsrc = 0;

    ELOG_DEBUG("Generating internal RtpMap");

    RtpMap vp8;
    vp8.payloadType = VP8_90000_PT;
    vp8.encodingName = "VP8";
    vp8.clockRate = 90000;
    vp8.channels = 1;
    vp8.mediaType = VIDEO_TYPE;
    internalPayloadVector_.push_back(vp8);

    RtpMap red;
    red.payloadType = RED_90000_PT;
    red.encodingName = "red";
    red.clockRate = 90000;
    red.channels = 1;
    red.mediaType = VIDEO_TYPE;
    internalPayloadVector_.push_back(red);

    RtpMap ulpfec;
    ulpfec.payloadType = ULP_90000_PT;
    ulpfec.encodingName = "ulpfec";
    ulpfec.clockRate = 90000;
    ulpfec.channels = 1;
    ulpfec.mediaType = VIDEO_TYPE;
    internalPayloadVector_.push_back(ulpfec);
/*
    RtpMap opus;
    opus.payloadType = OPUS_48000_PT;
    opus.encodingName = "opus";
    opus.clockRate = 48000;
    opus.channels = 2;
    opus.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(opus);

    RtpMap isac16;
    isac16.payloadType = ISAC_16000_PT;
    isac16.encodingName = "ISAC";
    isac16.clockRate = 16000;
    isac16.channels = 1;
    isac16.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(isac16);

    RtpMap isac32;
    isac32.payloadType = ISAC_32000_PT;
    isac32.encodingName = "ISAC";
    isac32.clockRate = 32000;
    isac32.channels = 1;
    isac32.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(isac32);
*/
    RtpMap pcmu;
    pcmu.payloadType = PCMU_8000_PT;
    pcmu.encodingName = "PCMU";
    pcmu.clockRate = 8000;
    pcmu.channels = 1;
    pcmu.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(pcmu);
/*
    RtpMap pcma;
    pcma.payloadType = PCMA_8000_PT;
    pcma.encodingName = "PCMA";
    pcma.clockRate = 8000;
    pcma.channels = 1;
    pcma.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(pcma);

    RtpMap cn8;
    cn8.payloadType = CN_8000_PT;
    cn8.encodingName = "CN";
    cn8.clockRate = 8000;
    cn8.channels = 1;
    cn8.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(cn8);

    RtpMap cn16;
    cn16.payloadType = CN_16000_PT;
    cn16.encodingName = "CN";
    cn16.clockRate = 16000;
    cn16.channels = 1;
    cn16.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(cn16);

    RtpMap cn32;
    cn32.payloadType = CN_32000_PT;
    cn32.encodingName = "CN";
    cn32.clockRate = 32000;
    cn32.channels = 1;
    cn32.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(cn32);

    RtpMap cn48;
    cn48.payloadType = CN_48000_PT;
    cn48.encodingName = "CN";
    cn48.clockRate = 48000;
    cn48.channels = 1;
    cn48.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(cn48);
*/
    RtpMap telephoneevent;
    telephoneevent.payloadType = TEL_8000_PT;
    telephoneevent.encodingName = "telephone-event";
    telephoneevent.clockRate = 8000;
    telephoneevent.channels = 1;
    telephoneevent.mediaType = AUDIO_TYPE;
    internalPayloadVector_.push_back(telephoneevent);

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
    char msidtemp [10];
    gen_random(msidtemp,10);

    ELOG_DEBUG("Getting SDP");

    std::ostringstream sdp;
    sdp << "v=0\n" << "o=- 0 0 IN IP4 127.0.0.1\n" << "s=\n" << "t=0 0\n";

    if (isBundle) {
      sdp << "a=group:BUNDLE audio video\n";
      sdp << "a=msid-semantic: WMS "<< msidtemp << endl;
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
        case SRFLX:
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
          for (unsigned int it =0; it<internalPayloadVector_.size(); it++){
            const RtpMap& payload_info = internalPayloadVector_[it];
            if (payload_info.mediaType == AUDIO_TYPE)
              sdp << payload_info.payloadType <<" ";

          }
          sdp << "\n"
            << "c=IN IP4 " << cand.hostAddress
            << endl;
          if (isRtcpMux) {
            sdp << "a=rtcp:" << candidateVector_[0].hostPort << " IN IP4 " << cand.hostAddress
                << endl;
          }
          printedAudio = true;
        }

        std::string generation = " generation 0";

        int comps = cand.componentId;
        if (isRtcpMux) {
          comps++;
        }
        for (int idx = cand.componentId; idx <= comps; idx++) {
          sdp << "a=candidate:" << cand.foundation << " " << idx
              << " " << cand.netProtocol << " " << cand.priority << " "
              << cand.hostAddress << " " << cand.hostPort << " typ "
              << hostType_str;
          if (cand.hostType == SRFLX || cand.hostType == RELAY) {
            //raddr 192.168.0.12 rport 50483
            sdp << " raddr " << cand.rAddress << " rport " << cand.rPort;
          }
          sdp << generation  << endl;
        }

        iceUsername_ = cand.username;
        icePassword_ = cand.password;
      }
    }
    //crypto audio
    if (printedAudio) {
      sdp << "a=ice-ufrag:" << iceUsername_ << endl;
      sdp << "a=ice-pwd:" << icePassword_ << endl;
      //sdp << "a=ice-options:google-ice" << endl;
      if (isFingerprint) {
        sdp << "a=fingerprint:sha-256 "<< fingerprint << endl;
      }
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

      for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
        const RtpMap& rtp = internalPayloadVector_[it];
        if (rtp.mediaType==AUDIO_TYPE) {
          if (rtp.channels>1) {
            sdp << "a=rtpmap:"<<rtp.payloadType << " " << rtp.encodingName << "/"
              << rtp.clockRate << "/" << rtp.channels << endl;
          } else {
            sdp << "a=rtpmap:"<<rtp.payloadType << " " << rtp.encodingName << "/"
              << rtp.clockRate << endl;
          }
          if(rtp.encodingName == "opus"){
            sdp << "a=fmtp:"<< rtp.payloadType<<" minptime=10\n";
          }
        }
      }
      if (audioSsrc == 0) {
        audioSsrc = 44444;
      }
      sdp << "a=maxptime:60" << endl;
      sdp << "a=ssrc:" << audioSsrc << " cname:o/i14u9pJrxRKAsu" << endl<<
        "a=ssrc:"<< audioSsrc << " msid:"<< msidtemp << " a0"<< endl<<
        "a=ssrc:"<< audioSsrc << " mslabel:"<< msidtemp << endl<<
        "a=ssrc:"<< audioSsrc << " label:" << msidtemp <<"a0"<<endl;

    }

    for (unsigned int it = 0; it < candidateVector_.size(); it++) {
      const CandidateInfo& cand = candidateVector_[it];
      std::string hostType_str;
      switch (cand.hostType) {
        case HOST:
          hostType_str = "host";
          break;
        case SRFLX:
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

          for (unsigned int it =0; it<internalPayloadVector_.size(); it++){
            const RtpMap& payload_info = internalPayloadVector_[it];
            if (payload_info.mediaType == VIDEO_TYPE)
              sdp << payload_info.payloadType <<" ";
          }

          sdp << "\n" << "c=IN IP4 " << cand.hostAddress << endl;
          if (isRtcpMux) {
            sdp << "a=rtcp:" << candidateVector_[0].hostPort << " IN IP4 " << cand.hostAddress
                << endl;
          }
          printedVideo = true;
        }

        std::string generation = " generation 0";
        int comps = cand.componentId;
        if (isRtcpMux) {
          comps++;
        }
        for (int idx = cand.componentId; idx <= comps; idx++) {
          sdp << "a=candidate:" << cand.foundation << " " << idx
              << " " << cand.netProtocol << " " << cand.priority << " "
              << cand.hostAddress << " " << cand.hostPort << " typ "
              << hostType_str;
          if (cand.hostType == SRFLX||cand.hostType == RELAY) {
            //raddr 192.168.0.12 rport 50483
            sdp << " raddr " << cand.rAddress << " rport " << cand.rPort;
          }
          sdp << generation  << endl;
        }

        iceUsername_ = cand.username;
        icePassword_ = cand.password;
      }
    }
    //crypto video
    if (printedVideo) {
      sdp << "a=ice-ufrag:" << iceUsername_ << endl;
      sdp << "a=ice-pwd:" << icePassword_ << endl;
      //sdp << "a=ice-options:google-ice" << endl;

      sdp << "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset" << endl;
      sdp << "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time" << endl;

      if (isFingerprint) {
        sdp << "a=fingerprint:sha-256 "<< fingerprint << endl;
      }
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

      for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
        const RtpMap& rtp = internalPayloadVector_[it];
          if (rtp.mediaType==VIDEO_TYPE)
          {
          sdp << "a=rtpmap:"<<rtp.payloadType << " " << rtp.encodingName << "/"
              << rtp.clockRate <<"\n";
          if(rtp.encodingName == "VP8"){
            sdp << "a=rtcp-fb:"<< rtp.payloadType<<" ccm fir\na=rtcp-fb:"<< rtp.payloadType<<" nack\na=rtcp-fb:" << rtp.payloadType<<" goog-remb\n" ;
          }
        }
      }
      if (videoSsrc == 0) {
        videoSsrc = 55543;
      }
      sdp << "a=ssrc:" << videoSsrc << " cname:o/i14u9pJrxRKAsu" << endl<<
        "a=ssrc:"<< videoSsrc << " msid:"<< msidtemp << " v0"<< endl<<
        "a=ssrc:"<< videoSsrc << " mslabel:"<< msidtemp << endl<<
        "a=ssrc:"<< videoSsrc << " label:" << msidtemp <<"v0"<<endl;
    }
    ELOG_DEBUG("sdp local \n %s",sdp.str().c_str());
    return sdp.str();
  }

  RtpMap *SdpInfo::getCodecByName(const std::string codecName, const unsigned int clockRate) {
    for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
      RtpMap& rtp = internalPayloadVector_[it];
      if (rtp.encodingName == codecName && rtp.clockRate == clockRate) {
        return &rtp;
      }
    }
    return NULL;
  }

  bool SdpInfo::supportCodecByName(const std::string codecName, const unsigned int clockRate) {
    RtpMap *rtp = getCodecByName(codecName, clockRate);
    if (rtp != NULL) {
      return supportPayloadType(rtp->payloadType);
    }
    return false;
  }

  bool SdpInfo::supportPayloadType(const int payloadType) {
    if (inOutPTMap.count(payloadType) > 0) {

      for (unsigned int it = 0; it < payloadVector_.size(); it++) {
        const RtpMap& rtp = payloadVector_[it];
        if (inOutPTMap[rtp.payloadType] == payloadType) {
          return true;
        }
      }

    }
    return false;
  }

  bool SdpInfo::processSdp(const std::string& sdp) {

    std::string strLine;
    std::istringstream iss(sdp);

    char line[500];
    char* pieces[100];
    char* cryptopiece[100];
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
      char* isFP = strstr(line,fp);

      if (isRtcpMuxchar){
        isRtcpMux = true;
      }
      if (isSAVPF){
        profile = SAVPF;
        ELOG_DEBUG("PROFILE %s (1 SAVPF)", isSAVPF);
      }
      if (isFP){
        char *pch;
        pch = strtok(line, ":");
        pch = strtok(NULL, " ");
        pch = strtok(NULL, " ");
        fingerprint = std::string(pch);
        isFingerprint = true;
        ELOG_DEBUG("Fingerprint %s ", fingerprint.c_str());
      }
      if (isGroup) {
        isBundle = true;
      }
      if (isVideo) {
        mtype = VIDEO_TYPE;
        hasVideo = true;
      }
      if (isAudio) {
        mtype = AUDIO_TYPE;
        hasAudio = true;
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
        //	ELOG_DEBUG("crypt %s", isCrypt );
        CryptoInfo crypinfo;
        char *pch;
        pch = strtok(line, " :");
        cryptopiece[0] = pch;
        int i = 0;
        while (pch != NULL) {
          pch = strtok(NULL, " :");
          //				ELOG_DEBUG("cryptopiece %i es %s", i, pch);
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

        bool found = false;
        for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
          const RtpMap& rtp = internalPayloadVector_[it];
          if (rtp.encodingName == codecname && rtp.clockRate == clock) {
            outInPTMap[PT] = rtp.payloadType;
            inOutPTMap[rtp.payloadType] = PT;
            found = true;
            ELOG_DEBUG("Mapping %s/%d:%d to %s/%d:%d", codecname.c_str(), clock, PT, rtp.encodingName.c_str(), rtp.clockRate, rtp.payloadType);
          }
        }
        if (found) {
          payloadVector_.push_back(theMap);
        }
      }

    }

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
    static const char* types_str[] = { "host", "srflx", "prflx", "relay" };
    cand.mediaType = mediaType;
    cand.foundation = pieces[0];
    cand.componentId = (unsigned int) strtoul(pieces[1], NULL, 10);

    cand.netProtocol = pieces[2];
    // libnice does not support tcp candidates, we ignore them
    if (cand.netProtocol.compare("UDP") && cand.netProtocol.compare("udp")) {
      return false;
    }
    //	a=candidate:0 1 udp 2130706432 1383 52314 typ host  generation 0
    //		        0 1 2    3            4          5     6  7    8          9
    // 
    // a=candidate:1367696781 1 udp 33562367 138. 49462 typ relay raddr 138.4 rport 53531 generation 0
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
        cand.hostType = SRFLX;
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

    if (cand.hostType == SRFLX || cand.hostType==RELAY) {
      cand.rAddress = std::string(pieces[9]);
      cand.rPort = (unsigned int) strtoul(pieces[11], NULL, 10);
      ELOG_DEBUG("Parsing raddr srlfx or relay %s, %u \n", cand.rAddress.c_str(), cand.rPort);
    }
    candidateVector_.push_back(cand);
    return true;
  }

  void SdpInfo::gen_random(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
  }
}/* namespace erizo */

