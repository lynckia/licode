/*
 * SDPProcessor.cpp
 */
#include "SdpInfo.h"

#include <stdio.h>

#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "rtp/RtpHeaders.h"
#include "./StringUtil.h"

using std::endl;
namespace erizo {
  DEFINE_LOGGER(SdpInfo, "SdpInfo");


  static const char *SDP_IDENTIFIER = "LicodeMCU";
  static const char *cand = "a=candidate:";
  static const char *crypto = "a=crypto:";
  static const char *group = "a=group:";
  static const char *video = "m=video";
  static const char *audio = "m=audio";
  static const char *mid = "a=mid";
  static const char *sendrecv = "a=sendrecv";
  static const char *recvonly = "a=recvonly";
  static const char *sendonly = "a=sendonly";
  static const char *ice_user = "a=ice-ufrag";
  static const char *ice_pass = "a=ice-pwd";
  static const char *ssrctag = "a=ssrc:";
  static const char *ssrcgrouptag = "a=ssrc-group";
  static const char *savpf = "SAVPF";
  static const char *rtpmap = "a=rtpmap:";
  static const char *rtcpmux = "a=rtcp-mux";
  static const char *fp = "a=fingerprint";
  static const char *setup = "a=setup:";
  static const char *extmap = "a=extmap:";
  static const char *rtcpfb = "a=rtcp-fb:";
  static const char *fmtp = "a=fmtp:";
  static const char *bas = "b=AS:";
  static const std::string kAssociatedPt = "apt";
  static const std::string kSimulcastGroup = "SIM";
  static const std::string kFidGroup = "FID";


  SdpInfo::SdpInfo(const std::vector<RtpMap> rtp_mappings) : internalPayloadVector_{rtp_mappings} {
    isBundle = false;
    isRtcpMux = false;
    isFingerprint = false;
    dtlsRole = ACTPASS;
    hasAudio = false;
    hasVideo = false;
    profile = SAVPF;
    audio_ssrc = 0;
    videoCodecs = 0;
    audioCodecs = 0;
    videoSdpMLine = -1;
    audioSdpMLine = -1;
    videoBandwidth = 0;
  }

  SdpInfo::~SdpInfo() {
  }

  bool SdpInfo::initWithSdp(const std::string& sdp, const std::string& media) {
    return processSdp(sdp, media);
  }
  std::string SdpInfo::addCandidate(const CandidateInfo& info) {
    candidateVector_.push_back(info);
    return stringifyCandidate(info);
  }

  std::string SdpInfo::stringifyCandidate(const CandidateInfo & candidate) {
    std::string generation = " generation 0";
    std::string hostType_str;
    std::ostringstream sdp;
    switch (candidate.hostType) {
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
    sdp << "a=candidate:" << candidate.foundation << " " << candidate.componentId
      << " " << candidate.netProtocol << " " << candidate.priority << " "
      << candidate.hostAddress << " " << candidate.hostPort << " typ "
      << hostType_str;

    if (candidate.hostType == SRFLX || candidate.hostType == RELAY) {
      // raddr 192.168.0.12 rport 50483
      sdp << " raddr " << candidate.rAddress << " rport " << candidate.rPort;
    }

    sdp << generation;
    return sdp.str();
  }

  void SdpInfo::addCrypto(const CryptoInfo& info) {
    cryptoVector_.push_back(info);
  }

  void SdpInfo::updateSupportedExtensionMap(const std::vector<ExtMap> &ext_map) {
    supported_ext_map_ = ext_map;
  }

  bool SdpInfo::isValidExtension(std::string uri) {
    auto value = std::find_if(supported_ext_map_.begin(), supported_ext_map_.end(), [uri](const ExtMap &extension) {
      return extension.uri == uri;
    });
    return value != supported_ext_map_.end();
  }

  void SdpInfo::setCredentials(const std::string& username, const std::string& password, MediaType media) {
    switch (media) {
      case(VIDEO_TYPE):
        iceVideoUsername_ = std::string(username);
        iceVideoPassword_ = std::string(password);
        break;
      case(AUDIO_TYPE):
        iceAudioUsername_ = std::string(username);
        iceAudioPassword_ = std::string(password);
        break;
      default:
        iceVideoUsername_ = std::string(username);
        iceVideoPassword_ = std::string(password);
        iceAudioUsername_ = std::string(username);
        iceAudioPassword_ = std::string(password);
        break;
    }
  }

  std::string SdpInfo::getUsername(MediaType media) const {
    if (media == AUDIO_TYPE) {
      return iceAudioUsername_;
    }
    return iceVideoUsername_;
  }

  std::string SdpInfo::getPassword(MediaType media) const {
    if (media == AUDIO_TYPE) {
      return iceAudioPassword_;
    }
    return iceVideoPassword_;
  }

  std::string SdpInfo::getSdp() {
    char msidtemp[11];
    gen_random(msidtemp, 10);

    ELOG_DEBUG("Getting SDP");

    std::ostringstream sdp;
    sdp << "v=0\n" << "o=- 0 0 IN IP4 127.0.0.1\n";
    sdp << "s=" << SDP_IDENTIFIER << "\n";
    sdp << "t=0 0\n";

    if (isBundle) {
      sdp << "a=group:BUNDLE";
      /*
         if (this->hasAudio)
         sdp << " audio";
         if (this->hasVideo)
         sdp << " video";
         */
      for (uint8_t i = 0; i < bundleTags.size(); i++) {
        sdp << " " << bundleTags[i].id;
      }
      sdp << "\n";
      sdp << "a=msid-semantic: WMS " << msidtemp << endl;
    }
    // candidates audio
    bool printedAudio = true, printedVideo = true;

    if (printedAudio && this->hasAudio) {
      sdp << "m=audio 1";
      if (profile == SAVPF) {
        sdp << " UDP/TLS/RTP/SAVPF ";
      } else {
        sdp << " RTP/AVPF ";  // << "103 104 0 8 106 105 13 126\n"
      }

      int codecCounter = 0;
      for (unsigned int it = 0; it < payloadVector.size(); it++) {
        const RtpMap& payload_info = payloadVector[it];
        if (payload_info.media_type == AUDIO_TYPE) {
          codecCounter++;
          sdp << payload_info.payload_type << ((codecCounter < audioCodecs) ? " " : "");
        }
      }

      sdp << "\n"
          << "c=IN IP4 0.0.0.0" << endl;
      if (isRtcpMux) {
        sdp << "a=rtcp:1 IN IP4 0.0.0.0" << endl;
      }
      for (unsigned int it = 0; it < candidateVector_.size(); it++) {
        if (candidateVector_[it].mediaType == AUDIO_TYPE || isBundle)
          sdp << this->stringifyCandidate(candidateVector_[it]) << endl;
      }
      if (iceAudioUsername_.size() > 0) {
        sdp << "a=ice-ufrag:" << iceAudioUsername_ << endl;
        sdp << "a=ice-pwd:" << iceAudioPassword_ << endl;
      } else {
        sdp << "a=ice-ufrag:" << iceVideoUsername_ << endl;
        sdp << "a=ice-pwd:" << iceVideoPassword_ << endl;
      }
      // sdp << "a=ice-options:google-ice" << endl;
      if (isFingerprint) {
        sdp << "a=fingerprint:sha-256 "<< fingerprint << endl;
      }
      switch (dtlsRole) {
        case ACTPASS:
          sdp << "a=setup:actpass"<< endl;
          break;
        case PASSIVE:
          sdp << "a=setup:passive"<< endl;
          break;
        case ACTIVE:
          sdp << "a=setup:active"<< endl;
          break;
      }
      switch (this->audioDirection) {
        case SENDONLY:
          sdp << "a=sendonly" << endl;
          break;
        case SENDRECV:
          sdp << "a=sendrecv" << endl;
          break;
        case RECVONLY:
          sdp << "a=recvonly" << endl;
          break;
      }

      ELOG_DEBUG("Writing Extmap for AUDIO %lu", extMapVector.size());
      for (uint8_t i = 0; i < extMapVector.size(); i++) {
        if (extMapVector[i].mediaType == AUDIO_TYPE && isValidExtension(std::string(extMapVector[i].uri))) {
          sdp << "a=extmap:" << extMapVector[i].value << " " << extMapVector[i].uri << endl;
        }
      }

      if (bundleTags.size() > 2) {
        ELOG_WARN("More bundleTags than supported, expect unexpected behaviour");
      }
      for (uint8_t i = 0; i < bundleTags.size(); i++) {
        if (bundleTags[i].mediaType == AUDIO_TYPE) {
          sdp << "a=mid:" << bundleTags[i].id << endl;
        }
      }
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

      for (unsigned int it = 0; it < payloadVector.size(); it++) {
        const RtpMap& rtp = payloadVector[it];
        if (rtp.media_type == AUDIO_TYPE) {
          int payload_type = rtp.payload_type;
          if (rtp.channels > 1) {
            sdp << "a=rtpmap:"<< payload_type << " " << rtp.encoding_name << "/"
              << rtp.clock_rate << "/" << rtp.channels << endl;
          } else {
            sdp << "a=rtpmap:"<< payload_type << " " << rtp.encoding_name << "/"
              << rtp.clock_rate << endl;
          }
          if (!rtp.feedback_types.empty()) {
            for (unsigned int itFb = 0; itFb < rtp.feedback_types.size(); itFb++) {
              sdp << "a=rtcp-fb:" << payload_type << " " << rtp.feedback_types[itFb] << "\n";
            }
          }
          for (std::map<std::string, std::string>::const_iterator theIt = rtp.format_parameters.begin();
              theIt != rtp.format_parameters.end(); theIt++) {
            if (theIt->first.compare("none")) {
              sdp << "a=fmtp:" << payload_type << " " << theIt->first << "=" << theIt->second << endl;
            } else {
              sdp << "a=fmtp:" << payload_type << " " << theIt->second << endl;
            }
          }
        }
      }

      if (audio_ssrc == 0) {
        audio_ssrc = 44444;
      }
      if (audioDirection != RECVONLY) {
        sdp << "a=ssrc:" << audio_ssrc << " cname:o/i14u9pJrxRKAsu" << endl <<
          "a=ssrc:"<< audio_ssrc << " msid:"<< msidtemp << " a0"<< endl <<
          "a=ssrc:"<< audio_ssrc << " mslabel:"<< msidtemp << endl <<
          "a=ssrc:"<< audio_ssrc << " label:" << msidtemp << "a0" << endl;
      }
    }

    if (printedVideo && this->hasVideo) {
      sdp << "m=video 1";
      if (profile == SAVPF) {
        sdp << " UDP/TLS/RTP/SAVPF ";
      } else {
        sdp << " RTP/AVPF ";
      }

      int codecCounter = 0;
      for (unsigned int it = 0; it < payloadVector.size(); it++) {
        const RtpMap& payload_info = payloadVector[it];
        if (payload_info.media_type == VIDEO_TYPE) {
          codecCounter++;
          sdp << payload_info.payload_type << ((codecCounter < videoCodecs) ? " " : "");
        }
      }

      sdp << "\n" << "c=IN IP4 0.0.0.0" << endl;
      if (isRtcpMux) {
        sdp << "a=rtcp:1 IN IP4 0.0.0.0" << endl;
      }
      for (unsigned int it = 0; it < candidateVector_.size(); it++) {
        if (candidateVector_[it].mediaType == VIDEO_TYPE)
          sdp << this->stringifyCandidate(candidateVector_[it]) << endl;
      }

      sdp << "a=ice-ufrag:" << iceVideoUsername_ << endl;
      sdp << "a=ice-pwd:" << iceVideoPassword_ << endl;
      // sdp << "a=ice-options:google-ice" << endl;

      ELOG_DEBUG("Writing Extmap for VIDEO %lu", extMapVector.size());
      for (uint8_t i = 0; i < extMapVector.size(); i++) {
        if (extMapVector[i].mediaType == VIDEO_TYPE && isValidExtension(std::string(extMapVector[i].uri))) {
          sdp << "a=extmap:" << extMapVector[i].value << " " << extMapVector[i].uri << endl;
        }
      }

      if (isFingerprint) {
        sdp << "a=fingerprint:sha-256 "<< fingerprint << endl;
      }
      switch (dtlsRole) {
        case ACTPASS:
          sdp << "a=setup:actpass"<< endl;
          break;
        case PASSIVE:
          sdp << "a=setup:passive"<< endl;
          break;
        case ACTIVE:
          sdp << "a=setup:active"<< endl;
          break;
      }
      switch (this->videoDirection) {
        case SENDONLY:
          sdp << "a=sendonly" << endl;
          break;
        case SENDRECV:
          sdp << "a=sendrecv" << endl;
          break;
        case RECVONLY:
          sdp << "a=recvonly" << endl;
          break;
      }
      for (uint8_t i = 0; i < bundleTags.size(); i++) {
        if (bundleTags[i].mediaType == VIDEO_TYPE) {
          sdp << "a=mid:" << bundleTags[i].id << endl;
        }
      }
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

      for (unsigned int it = 0; it < payloadVector.size(); it++) {
        const RtpMap& rtp = payloadVector[it];
        if (rtp.media_type == VIDEO_TYPE) {
          int payload_type = rtp.payload_type;
          sdp << "a=rtpmap:" << payload_type << " " << rtp.encoding_name << "/"
              << rtp.clock_rate <<"\n";
          if (!rtp.feedback_types.empty()) {
            for (unsigned int itFb = 0; itFb < rtp.feedback_types.size(); itFb++) {
              sdp << "a=rtcp-fb:" << payload_type << " " << rtp.feedback_types[itFb] << "\n";
            }
          }
          for (std::map<std::string, std::string>::const_iterator theIt = rtp.format_parameters.begin();
              theIt != rtp.format_parameters.end(); theIt++) {
            if (theIt->first.compare("none")) {
              sdp << "a=fmtp:" << payload_type << " " << theIt->first << "=" << theIt->second << endl;
            } else {
              sdp << "a=fmtp:" << payload_type << " " << theIt->second << endl;
            }
          }
        }
      }
      if (video_ssrc_list.empty()) {
        video_ssrc_list.push_back(55543);
      }
      if (videoDirection != RECVONLY) {
        std::for_each(video_ssrc_list.begin(), video_ssrc_list.end(),
            [&sdp, &msidtemp](uint32_t &video_ssrc){
            sdp << "a=ssrc:" << video_ssrc << " cname:o/i14u9pJrxRKAsu" << endl <<
            "a=ssrc:" << video_ssrc << " msid:"<< msidtemp << " v0"<< endl <<
            "a=ssrc:" << video_ssrc << " mslabel:"<< msidtemp << endl <<
            "a=ssrc:" << video_ssrc << " label:" << msidtemp << "v0" << endl;
            });
        /* TODO(pedro)  properly encode FID groups in sdp when supported
        std::for_each(video_rtx_ssrc_map.begin(), video_rtx_ssrc_map.end(),
            [&sdp, &msidtemp](uint32_t &video_rtx_ssrc){
            sdp << "a=ssrc:" << video_rtx_ssrc << " cname:o/i14u9pJrxRKAsu" << endl <<
            "a=ssrc:" << video_rtx_ssrc << " msid:"<< msidtemp << " v0"<< endl <<
            "a=ssrc:" << video_rtx_ssrc << " mslabel:"<< msidtemp << endl <<
            "a=ssrc:" << video_rtx_ssrc << " label:" << msidtemp << "v0" << endl;
            });
        */
      }
    }
    ELOG_DEBUG("sdp local \n %s", sdp.str().c_str());
    return sdp.str();
  }

  RtpMap* SdpInfo::getCodecByExternalPayloadType(const unsigned int payload_type) {
    for (unsigned int it = 0; it < payloadVector.size(); it++) {
      RtpMap& rtp = payloadVector[it];
      if (rtp.payload_type == payload_type) {
        return &rtp;
      }
    }
    return nullptr;
  }

  RtpMap *SdpInfo::getCodecByName(const std::string codecName, const unsigned int clockRate) {
    for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
      RtpMap& rtp = internalPayloadVector_[it];
      if (rtp.encoding_name == codecName && rtp.clock_rate == clockRate) {
        return &rtp;
      }
    }
    return NULL;
  }

  bool SdpInfo::supportCodecByName(const std::string codecName, const unsigned int clockRate) {
    RtpMap *rtp = getCodecByName(codecName, clockRate);
    if (rtp != NULL) {
      return supportPayloadType(rtp->payload_type);
    }
    return false;
  }

  bool SdpInfo::supportPayloadType(const unsigned int payloadType) {
    if (inOutPTMap.count(payloadType) > 0) {
      for (unsigned int it = 0; it < payloadVector.size(); it++) {
        const RtpMap& rtp = payloadVector[it];
        if (inOutPTMap[rtp.payload_type] == payloadType) {
          return true;
        }
      }
    }
    return false;
  }

  // TODO(pedro): Should provide hints
  void SdpInfo::createOfferSdp(bool videoEnabled, bool audioEnabled, bool bundle) {
    ELOG_DEBUG("Creating offerSDP: video %d, audio %d, bundle %d", videoEnabled, audioEnabled, bundle);
    this->payloadVector = internalPayloadVector_;
    this->isBundle = bundle;
    this->profile = SAVPF;
    this->isRtcpMux = true;
    if (videoEnabled)
      this->videoSdpMLine = 0;
    if (audioEnabled)
      this->audioSdpMLine = 0;

    for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
      RtpMap& rtp = internalPayloadVector_[it];
      if (rtp.media_type == VIDEO_TYPE) {
        videoCodecs++;
      } else if (rtp.media_type == AUDIO_TYPE) {
        audioCodecs++;
      }
    }

    this->hasVideo = videoEnabled;
    this->hasAudio = audioEnabled;
    this->videoDirection = SENDRECV;
    this->audioDirection = SENDRECV;
    ELOG_DEBUG("Setting Offer SDP");
  }

  void SdpInfo::setOfferSdp(const SdpInfo& offerSdp) {
    this->videoCodecs = offerSdp.videoCodecs;
    this->audioCodecs = offerSdp.audioCodecs;
    this->payloadVector = offerSdp.payloadVector;
    this->isBundle = offerSdp.isBundle;
    this->profile = offerSdp.profile;
    this->isRtcpMux = offerSdp.isRtcpMux;
    this->videoSdpMLine = offerSdp.videoSdpMLine;
    this->audioSdpMLine = offerSdp.audioSdpMLine;
    this->inOutPTMap = offerSdp.inOutPTMap;
    this->outInPTMap = offerSdp.outInPTMap;
    this->hasVideo = offerSdp.hasVideo;
    this->hasAudio = offerSdp.hasAudio;
    this->bundleTags = offerSdp.bundleTags;
    this->extMapVector = offerSdp.extMapVector;
    switch (offerSdp.videoDirection) {
      case SENDONLY:
        this->videoDirection = RECVONLY;
        break;
      case RECVONLY:
        this->videoDirection = SENDONLY;
        break;
      case SENDRECV:
        this->videoDirection = SENDRECV;
        break;
      default:
        this->videoDirection = SENDRECV;
        break;
    }
    switch (offerSdp.audioDirection) {
      case SENDONLY:
        this->audioDirection = RECVONLY;
        break;
      case RECVONLY:
        this->audioDirection = SENDONLY;
        break;
      case SENDRECV:
        this->audioDirection = SENDRECV;
        break;
      default:
        this->audioDirection = SENDRECV;
        break;
    }
    ELOG_DEBUG("Offer SDP successfully set");
  }

  bool SdpInfo::processSdp(const std::string& sdp, const std::string& media) {
    std::string line;
    std::istringstream iss(sdp);
    int mlineNum = -1;

    MediaType mtype = OTHER;
    if (media == "audio") {
      mtype = AUDIO_TYPE;
    } else if (media == "video") {
      mtype = VIDEO_TYPE;
    }

    while (std::getline(iss, line)) {
      size_t isVideo = line.find(video);
      size_t isAudio = line.find(audio);
      size_t isGroup = line.find(group);
      size_t isMid = line.find(mid);
      size_t isCand = line.find(cand);
      size_t isCrypt = line.find(crypto);
      size_t isUser = line.find(ice_user);
      size_t isPass = line.find(ice_pass);
      size_t isSsrc = line.find(ssrctag);
      size_t isSsrcGroup = line.find(ssrcgrouptag);
      size_t isSAVPF = line.find(savpf);
      size_t isRtpmap = line.find(rtpmap);
      size_t isRtcpMuxchar = line.find(rtcpmux);
      size_t isSendRecv = line.find(sendrecv);
      size_t isRecvOnly = line.find(recvonly);
      size_t isSendOnly = line.find(sendonly);
      size_t isFP = line.find(fp);
      size_t isSetup = line.find(setup);
      size_t isExtMap = line.find(extmap);
      size_t isFeedback = line.find(rtcpfb);
      size_t isFmtp = line.find(fmtp);
      size_t isBandwidth = line.find(bas);

      ELOG_DEBUG("current line -> %s", line.c_str());

      if (isRtcpMuxchar != std::string::npos) {
        isRtcpMux = true;
      }

      // At this point we support only one direction per SDP
      // Any other combination does not make sense at this point in Licode
      if (isRecvOnly != std::string::npos) {
        ELOG_DEBUG("RecvOnly sdp")
          if (mtype == AUDIO_TYPE) {
            this->audioDirection = RECVONLY;
          } else {
            this->videoDirection = RECVONLY;
          }
      } else if (isSendOnly != std::string::npos) {
        ELOG_DEBUG("SendOnly sdp")
          if (mtype == AUDIO_TYPE) {
            this->audioDirection = SENDONLY;
          } else {
            this->videoDirection = SENDONLY;
          }
      } else if (isSendRecv != std::string::npos) {
        if (mtype == AUDIO_TYPE) {
          this->audioDirection = SENDRECV;
        } else {
          this->videoDirection = SENDRECV;
        }
        ELOG_DEBUG("SendRecv sdp")
      }

      if (isSAVPF != std::string::npos) {
        profile = SAVPF;
        ELOG_DEBUG("PROFILE %s (1 SAVPF)", line.substr(isSAVPF).c_str());
      }
      if (isFP != std::string::npos) {
        std::vector<std::string> parts;

        // FIXME add error checking here
        parts = stringutil::splitOneOf(line, ":", 1);
        parts = stringutil::splitOneOf(parts[1], " ");

        fingerprint = parts[1];
        isFingerprint = true;
        ELOG_DEBUG("Fingerprint %s ", fingerprint.c_str());
      }
      if (isSetup != std::string::npos) {
        std::vector<std::string> parts;
        parts = stringutil::splitOneOf(line, ":", 1);
        if (parts[1].find("passive") != std::string::npos) {
          ELOG_DEBUG("Dtls passive");
          dtlsRole = PASSIVE;
        } else if (parts[1].find("active") != std::string::npos) {
          ELOG_DEBUG("Dtls active");
          dtlsRole = ACTIVE;
        } else {
          ELOG_DEBUG("Dtls actpass");
          dtlsRole = ACTPASS;
        }
      }
      if (isGroup != std::string::npos) {
        std::vector<std::string> parts;
        parts = stringutil::splitOneOf(line, ":  \r\n", 10);
        if (!parts[1].compare("BUNDLE")) {
          ELOG_DEBUG("BUNDLE sdp");
          isBundle = true;
        }
        // number of parts will vary depending on whether audio and video are present in the bundle
        if (parts.size() >= 3) {
          for (unsigned int tagno = 2; tagno < parts.size(); tagno++) {
            ELOG_DEBUG("Adding %s to bundle vector", parts[tagno].c_str());
            BundleTag theTag(parts[tagno], OTHER);
            bundleTags.push_back(theTag);
          }
        }
      }
      if (isVideo != std::string::npos) {
        videoSdpMLine = ++mlineNum;
        ELOG_DEBUG("sdp has video, mline = %d", videoSdpMLine);
        mtype = VIDEO_TYPE;
        hasVideo = true;
      }
      if (isAudio != std::string::npos) {
        audioSdpMLine = ++mlineNum;
        ELOG_DEBUG("sdp has audio, mline = %d", audioSdpMLine);
        mtype = AUDIO_TYPE;
        hasAudio = true;
      }
      if (isCand != std::string::npos) {
        std::vector<std::string> pieces = stringutil::splitOneOf(line, " :");
        processCandidate(pieces, mtype);
      }
      if (isCrypt != std::string::npos) {
        CryptoInfo crypinfo;
        std::vector<std::string> cryptopiece = stringutil::splitOneOf(line, " :");

        // FIXME: add error checking here
        crypinfo.cipherSuite = cryptopiece[2];
        crypinfo.keyParams = cryptopiece[4];
        crypinfo.mediaType = mtype;
        cryptoVector_.push_back(crypinfo);
        ELOG_DEBUG("Crypto Info: %s %s %d", crypinfo.cipherSuite.c_str(),
            crypinfo.keyParams.c_str(),
            crypinfo.mediaType);
      }
      if (isUser != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(stringutil::splitOneOf(line, ":", 1)[1], "\r", 1);
        // FIXME add error checking
        if (mtype == VIDEO_TYPE) {
          iceVideoUsername_ = parts[0];
          ELOG_DEBUG("ICE Video username: %s", iceVideoUsername_.c_str());
        } else if (mtype == AUDIO_TYPE) {
          iceAudioUsername_ = parts[0];
          ELOG_DEBUG("ICE Audio username: %s", iceAudioUsername_.c_str());
        } else {
          ELOG_DEBUG("Unknown media type for ICE credentials, looks like Firefox");
          iceVideoUsername_ = parts[0];
        }
      }
      if (isPass != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(stringutil::splitOneOf(line, ":", 1)[1], "\r", 1);
        // FIXME add error checking
        if (mtype == VIDEO_TYPE) {
          iceVideoPassword_ = parts[0];
          ELOG_DEBUG("ICE Video password: %s", iceVideoPassword_.c_str());
        } else if (mtype == AUDIO_TYPE) {
          iceAudioPassword_ = parts[0];
          ELOG_DEBUG("ICE Audio password: %s", iceAudioPassword_.c_str());
        } else {
          ELOG_DEBUG("Unknown media type for ICE credentials, looks like Firefox");
          iceVideoPassword_ = parts[0];
        }
      }
      if (isMid != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(line, ": \r\n", 4);
        if (parts.size() >= 2 && isBundle) {
          std::string thisId = parts[1];
          for (uint8_t i = 0; i < bundleTags.size(); i++) {
            if (!bundleTags[i].id.compare(thisId)) {
              ELOG_DEBUG("Setting tag %s to mediaType %d", thisId.c_str(), mtype);
              bundleTags[i].mediaType = mtype;
            }
          }
        } else {
          ELOG_WARN("Unexpected size of a=mid element");
        }
      }

      if (isSsrc != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :", 2);
        // FIXME add error checking
        if (mtype == VIDEO_TYPE) {
          uint32_t parsed_ssrc = strtoul(parts[1].c_str(), nullptr, 10);
          ELOG_DEBUG("message: maybeAdd video in isSsrc, ssrc: %u", parsed_ssrc);
          maybeAddSsrcToList(parsed_ssrc);
        } else if ((mtype == AUDIO_TYPE) && (audio_ssrc == 0)) {
          audio_ssrc = strtoul(parts[1].c_str(), nullptr, 10);
          ELOG_DEBUG("audio ssrc: %u", audio_ssrc);
        }
      }

      if (isSsrcGroup != std::string::npos) {
        if (mtype != VIDEO_TYPE) {
          continue;
        }
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :", 10);
        if (parts.size() < 4) {
          continue;
        }
        if (parts[1] == kSimulcastGroup) {
          ELOG_DEBUG("Detected SIM group, size: %lu", parts.size());
          std::for_each(parts.begin() + 2, parts.end(), [this] (std::string &part){
              uint32_t parsed_ssrc = strtoul(part.c_str(), nullptr, 10);
              ELOG_DEBUG("maybeAddSsrc video SIM, ssrc %u", parsed_ssrc);
              maybeAddSsrcToList(parsed_ssrc);
              });

        } else if (parts[1] == kFidGroup) {
          int number_of_ssrcs = parts.size() - 2;
          if (number_of_ssrcs != 2) {
            ELOG_DEBUG("FID Group with wrong number of SSRCs, ignoring");
            continue;
          }
          uint32_t original_ssrc = strtoul(parts[2].c_str(), nullptr, 10);
          uint32_t rtx_ssrc = strtoul(parts[3].c_str(), nullptr, 10);
          video_rtx_ssrc_map[rtx_ssrc] = original_ssrc;
          ELOG_DEBUG("message: parsed FID group, original_src: %u, rtx_ssrc: %u", original_ssrc, rtx_ssrc);
        }
      }
      // a=rtpmap:PT codec_name/clock_rate
      if (isRtpmap != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :/\n", 4);
        unsigned int PT = strtoul(parts[1].c_str(), nullptr, 10);
        std::string codecname = parts[2];
        unsigned int parsed_clock = strtoul(parts[3].c_str(), nullptr, 10);
        auto map_element = payload_parsed_map_.find(PT);
        if (map_element != payload_parsed_map_.end()) {
          ELOG_DEBUG("message: updating parsed ptmap to vector, PT: %u, name %s, clock %u",
              PT, codecname.c_str(), parsed_clock);
          map_element->second.payload_type = PT;
          map_element->second.encoding_name = codecname;
          map_element->second.clock_rate = parsed_clock;
          map_element->second.media_type = mtype;
        } else {
          ELOG_DEBUG("message: adding parsed ptmap to vector, PT: %u, name %s, clock %u",
              PT, codecname.c_str(), parsed_clock);
          RtpMap new_mapping;
          new_mapping.payload_type = PT;
          new_mapping.encoding_name = codecname;
          new_mapping.clock_rate = parsed_clock;
          new_mapping.media_type = mtype;
          payload_parsed_map_[PT] = new_mapping;
        }
      }
      // a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
      if (isExtMap != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :=", 3);
        if (parts.size() >= 3) {
          unsigned int id = strtoul(parts[2].c_str(), nullptr, 10);
          ExtMap anExt(id, parts[3].substr(0, parts[3].size()-1));
          anExt.mediaType = mtype;
          extMapVector.push_back(anExt);
        }
      }

      if (isFeedback != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :", 2);
        unsigned int PT = strtoul(parts[1].c_str(), nullptr, 10);
        std::string feedback = parts[2];
        feedback.pop_back();  // remove end of line
        auto map_element = payload_parsed_map_.find(PT);
        if (map_element != payload_parsed_map_.end()) {
          map_element->second.feedback_types.push_back(feedback);
        } else {
          RtpMap new_map;
          new_map.payload_type = PT;
          new_map.feedback_types.push_back(feedback);
          payload_parsed_map_[PT] = new_map;
        }
      }

      if (isFmtp != std::string::npos) {
        std::vector<std::string> parts = stringutil::splitOneOf(line, " :=", 4);
        if (parts.size() < 4) {
          continue;
        }
        unsigned int PT = strtoul(parts[2].c_str(), nullptr, 10);
        std::string option = "none";
        std::string value = "none";
        if (parts.size() == 4) {
          value = parts[3].c_str();
        } else {
          option = parts[3].c_str();
          value = parts[4].c_str();
        }
        ELOG_DEBUG("message: Parsing format parameter, option: %s, value: %s, PT: %u",
            option.c_str(), value.c_str(), PT);
        value.pop_back();
        auto map_element = payload_parsed_map_.find(PT);
        if (map_element != payload_parsed_map_.end()) {
          map_element->second.format_parameters[option] = value;
        } else {
          RtpMap new_map;
          new_map.payload_type = PT;
          new_map.format_parameters[option] = value;
          payload_parsed_map_[PT] = new_map;
        }
      }

      if (isBandwidth != std::string::npos) {
        if (mtype == VIDEO_TYPE) {
          std::vector<std::string> parts = stringutil::splitOneOf(line, ":", 2);
          if (parts.size() >= 2) {
            videoBandwidth = strtoul(parts[1].c_str(), nullptr, 10);
            ELOG_DEBUG("Bandwidth for video detected %u", videoBandwidth);
          }
        }
      }
    }  // sdp lines loop

    // If there is no video or audio credentials we use the ones we have
    if (iceVideoUsername_.empty() && iceAudioUsername_.empty()) {
      ELOG_ERROR("No valid credentials for ICE")
        return false;
    } else if (iceVideoUsername_.empty()) {
      ELOG_DEBUG("Video credentials empty, setting the audio ones");
      iceVideoUsername_ = iceAudioUsername_;
      iceVideoPassword_ = iceAudioPassword_;
    } else if (iceAudioUsername_.empty()) {
      ELOG_DEBUG("Audio credentials empty, setting the video ones");
      iceAudioUsername_ = iceVideoUsername_;
      iceAudioPassword_ = iceVideoPassword_;
    }

    for (unsigned int i = 0; i < candidateVector_.size(); i++) {
      CandidateInfo& c = candidateVector_[i];
      c.isBundle = isBundle;
      if (c.mediaType == VIDEO_TYPE) {
        c.username = iceVideoUsername_;
        c.password = iceVideoPassword_;
      } else {
        c.username = iceAudioUsername_;
        c.password = iceAudioPassword_;
      }
    }

    if (video_ssrc_list.empty()) {
      video_ssrc_list.push_back(0);
    }

    //  go through the payload_map_ and match it with internalPayloadVector_
    //  generate rtpMaps and payloadVector
    std::vector<RtpMap> rtx_maps;
    for (const RtpMap& internal_map : internalPayloadVector_) {
      for (const std::pair<const unsigned int, RtpMap>& parsed_pair : payload_parsed_map_) {
        const RtpMap& parsed_map = parsed_pair.second;
        if (internal_map.encoding_name != parsed_map.encoding_name ||
            internal_map.clock_rate != parsed_map.clock_rate) {
          continue;
        }
        if (parsed_map.encoding_name == "rtx") {  // we'll process those later when we have the pt maps
          rtx_maps.push_back(parsed_map);
          continue;
        }
        RtpMap negotiated_map(parsed_map);
        outInPTMap[parsed_map.payload_type] = internal_map.payload_type;
        inOutPTMap[internal_map.payload_type] = parsed_map.payload_type;
        negotiated_map.channels = internal_map.channels;
        ELOG_DEBUG("Mapping %s/%d:%d to %s/%d:%d",
            parsed_map.encoding_name.c_str(), parsed_map.clock_rate, parsed_map.payload_type,
            internal_map.encoding_name.c_str(), internal_map.clock_rate, internal_map.payload_type);


        ELOG_DEBUG("message: Checking feedback types, parsed: %lu, internal:%lu", parsed_map.feedback_types.size(),
            internal_map.feedback_types.size());

        std::vector<std::string> negotiated_feedback;
        if (!parsed_map.feedback_types.empty() && !internal_map.feedback_types.empty()) {
          for (const std::string& internal_feedback_line : internal_map.feedback_types) {
            for (const std::string& parsed_feedback_line : parsed_map.feedback_types) {
              if (internal_feedback_line == parsed_feedback_line) {
                ELOG_DEBUG("message: Adding feedback to codec, feedback: %s, encoding_name: %s",
                   internal_feedback_line.c_str(),
                   internal_map.encoding_name.c_str());
                negotiated_feedback.push_back(internal_feedback_line);
              }
            }
          }
        }
        negotiated_map.feedback_types = negotiated_feedback;
        std::map<std::string, std::string> negotiated_parameters;
        ELOG_DEBUG("message, Checking fmtp parameters, parsed: %lu, internal: %lu", parsed_map.format_parameters.size(),
            internal_map.format_parameters.size());
        if (!parsed_map.format_parameters.empty() && !internal_map.format_parameters.empty()) {
          for (const std::pair<std::string, std::string>& internal_parameter : internal_map.format_parameters) {
            auto found_parameter = parsed_map.format_parameters.find(internal_parameter.first);
            if (found_parameter != parsed_map.format_parameters.end()) {
              if (found_parameter->second == internal_parameter.second) {
                ELOG_DEBUG("message: Adding fmpt, codec_name: %s, parameter: %s, value:%s",
                    parsed_map.encoding_name.c_str(), found_parameter->first.c_str(),
                    found_parameter->second.c_str());
                negotiated_parameters[found_parameter->first] = found_parameter->second;
              }
            }
          }
        }

        negotiated_map.format_parameters = negotiated_parameters;

        if (negotiated_map.media_type == VIDEO_TYPE) {
          videoCodecs++;
        } else {
          audioCodecs++;
        }
        payloadVector.push_back(negotiated_map);
      }
    }

    //  Check atp rtx
    for (RtpMap& rtx_map : rtx_maps) {
      for (const RtpMap& internal_map : internalPayloadVector_) {
        if (internal_map.encoding_name == "rtx") {
            auto parsed_apt = rtx_map.format_parameters.find(kAssociatedPt);
            auto internal_apt = internal_map.format_parameters.find(kAssociatedPt);
            if (parsed_apt == rtx_map.format_parameters.end() &&
                internal_apt == internal_map.format_parameters.end()) {
              continue;
            }
            unsigned int internal_apt_pt = std::stoul(internal_apt->second);
            unsigned int parsed_apt_pt = std::stoul(parsed_apt->second);
            ELOG_DEBUG("message: looking for apt correspondence, internal_apt_pt: %u, parsed_apt_pt: %u",
                internal_apt_pt, parsed_apt_pt);
            if (outInPTMap[parsed_apt_pt] == internal_apt_pt) {
              ELOG_DEBUG("message: matched atp for rtx, internal_apt_pt: %u, parsed_apt_pt: %u",
                  internal_apt_pt, parsed_apt_pt);
              if (rtx_map.media_type == VIDEO_TYPE) {
                videoCodecs++;
              } else {
                audioCodecs++;
              }
              outInPTMap[rtx_map.payload_type] = internal_map.payload_type;
              inOutPTMap[internal_map.payload_type] = rtx_map.payload_type;
              payloadVector.push_back(rtx_map);
            }
        }
      }
    }
    return true;
  }

  std::vector<CandidateInfo>& SdpInfo::getCandidateInfos() {
    return candidateVector_;
  }

  std::vector<CryptoInfo>& SdpInfo::getCryptoInfos() {
    return cryptoVector_;
  }

  std::vector<RtpMap>& SdpInfo::getPayloadInfos() {
    return payloadVector;
  }

  unsigned int SdpInfo::getAudioInternalPT(unsigned int externalPT) {
    // should use separate mapping for video and audio at the very least
    // standard requires separate mappings for each media, even!
    std::map<unsigned int, unsigned int>::iterator found = outInPTMap.find(externalPT);
    if (found != outInPTMap.end()) {
      return found->second;
    }
    return externalPT;
  }

  unsigned int SdpInfo::getVideoInternalPT(unsigned int externalPT) {
    // WARNING
    // should use separate mapping for video and audio at the very least
    // standard requires separate mappings for each media, even!
    return getAudioInternalPT(externalPT);
  }

  unsigned int SdpInfo::getAudioExternalPT(unsigned int internalPT) {
    // should use separate mapping for video and audio at the very least
    // standard requires separate mappings for each media, even!
    std::map<unsigned int, unsigned int>::iterator found = inOutPTMap.find(internalPT);
    if (found != inOutPTMap.end()) {
      return found->second;
    }
    return internalPT;
  }

  unsigned int SdpInfo::getVideoExternalPT(unsigned int internalPT) {
    // WARNING
    // should use separate mapping for video and audio at the very least
    // standard requires separate mappings for each media, even!
    return getAudioExternalPT(internalPT);
  }

  bool SdpInfo::processCandidate(const std::vector<std::string>& pieces, MediaType mediaType) {
    CandidateInfo cand;
    static const char* types_str[] = { "host", "srflx", "prflx", "relay" };
    cand.mediaType = mediaType;
    cand.foundation = pieces[1];
    cand.componentId = (unsigned int) strtoul(pieces[2].c_str(), nullptr, 10);

    cand.netProtocol = pieces[3];
    // libnice does not support tcp candidates, we ignore them
    ELOG_DEBUG("cand.netProtocol=%s", cand.netProtocol.c_str());
    if (cand.netProtocol.compare("UDP") && cand.netProtocol.compare("udp")) {
      return false;
    }
    // a=candidate:0 1 udp 2130706432 1383 52314 typ host  generation 0
    //             0 1 2    3            4          5     6  7    8          9
    //
    // a=candidate:1367696781 1 udp 33562367 138. 49462 typ relay raddr 138.4 rport 53531 generation 0
    cand.priority = (unsigned int) strtoul(pieces[4].c_str(), nullptr, 10);
    cand.hostAddress = pieces[5];
    cand.hostPort = (unsigned int) strtoul(pieces[6].c_str(), nullptr, 10);
    if (pieces[7] != "typ") {
      return false;
    }
    unsigned int type = 1111;
    int p;
    for (p = 0; p < 4; p++) {
      if (pieces[8] == types_str[p]) {
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

    ELOG_DEBUG("Candidate Info: foundation=%s, componentId=%u, netProtocol=%s, "
        "priority=%u, hostAddress=%s, hostPort=%u, hostType=%u",
        cand.foundation.c_str(),
        cand.componentId,
        cand.netProtocol.c_str(),
        cand.priority,
        cand.hostAddress.c_str(),
        cand.hostPort,
        cand.hostType);

    if (cand.hostType == SRFLX || cand.hostType == RELAY) {
      cand.rAddress = pieces[10];
      cand.rPort = (unsigned int) strtoul(pieces[12].c_str(), nullptr, 10);
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

  void SdpInfo::maybeAddSsrcToList(uint32_t ssrc) {
    auto find_rt = video_rtx_ssrc_map.find(ssrc);
    if (find_rt != video_rtx_ssrc_map.end()) {
      //  Its a rtx ssrc
      return;
    }
    auto value = std::find_if(video_ssrc_list.begin(), video_ssrc_list.end(), [ssrc](uint32_t current_ssrc) {
        return ssrc == current_ssrc;
        });
    if (value == video_ssrc_list.end()) {
      ELOG_DEBUG("message: Adding ssrc to list, ssrc: %u", ssrc);
      video_ssrc_list.push_back(ssrc);
    }
  }

}  // namespace erizo
