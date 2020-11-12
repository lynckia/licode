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
#include <cassert>

#include "rtp/RtpHeaders.h"
#include "./StringUtil.h"

using std::endl;
namespace erizo {
  DEFINE_LOGGER(SdpInfo, "SdpInfo");

  static const std::string kAssociatedPt = "apt";
  static const std::string kSimulcastGroup = "SIM";
  static const std::string kFidGroup = "FID";


  SdpInfo::SdpInfo(const std::vector<RtpMap> rtp_mappings) : internalPayloadVector_{rtp_mappings} {
    isBundle = false;
    isRtcpMux = false;
    isFingerprint = false;
    dtlsRole = ACTPASS;
    internal_dtls_role = ACTPASS;
    hasAudio = false;
    hasVideo = false;
    profile = SAVPF;
    videoCodecs = 0;
    audioCodecs = 0;
    videoSdpMLine = -1;
    audioSdpMLine = -1;
    videoBandwidth = 0;
    google_conference_flag_set = "";
  }

  SdpInfo::~SdpInfo() {
  }

  std::string SdpInfo::addCandidate(const CandidateInfo& info) {
    candidateVector_.push_back(info);
    return info.to_string();
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
    ELOG_DEBUG("Creating offerSDP: video %d, audio %d, bundle %d, payloadVector: %d, extSize: %d",
      videoEnabled, audioEnabled, bundle, payloadVector.size(), extMapVector.size());
    if (payloadVector.size() == 0) {
      payloadVector = internalPayloadVector_;
    }

    this->isBundle = bundle;
    this->profile = SAVPF;
    this->isRtcpMux = true;
    if (videoEnabled)
      this->videoSdpMLine = 0;
    if (audioEnabled)
      this->audioSdpMLine = 0;

    for (unsigned int it = 0; it < payloadVector.size(); it++) {
      RtpMap& rtp = payloadVector[it];
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

  void SdpInfo::copyInfoFromSdp(std::shared_ptr<SdpInfo> offerSdp) {
    payloadVector = offerSdp->payloadVector;
    videoCodecs = offerSdp->videoCodecs;
    audioCodecs = offerSdp->audioCodecs;
    inOutPTMap = offerSdp->inOutPTMap;
    outInPTMap = offerSdp->outInPTMap;
    extMapVector = offerSdp->extMapVector;
    ELOG_DEBUG("Offer SDP successfully copied, extSize: %d, payloadSize: %d, videoCodecs: %d, audioCodecs: %d",
      extMapVector.size(), payloadVector.size(), videoCodecs, audioCodecs);
  }

  void SdpInfo::setOfferSdp(std::shared_ptr<SdpInfo> offerSdp) {
    this->videoCodecs = offerSdp->videoCodecs;
    this->audioCodecs = offerSdp->audioCodecs;
    this->payloadVector = offerSdp->payloadVector;
    this->isBundle = offerSdp->isBundle;
    this->profile = offerSdp->profile;
    this->isRtcpMux = offerSdp->isRtcpMux;
    this->videoSdpMLine = offerSdp->videoSdpMLine;
    this->audioSdpMLine = offerSdp->audioSdpMLine;
    this->inOutPTMap = offerSdp->inOutPTMap;
    this->outInPTMap = offerSdp->outInPTMap;
    this->hasVideo = offerSdp->hasVideo;
    this->hasAudio = offerSdp->hasAudio;
    this->bundleTags = offerSdp->bundleTags;
    this->extMapVector = offerSdp->extMapVector;
    this->rids_ = offerSdp->rids();
    this->google_conference_flag_set = offerSdp->google_conference_flag_set;
    for (auto& rid : rids_) {
      rid.direction = reverse(rid.direction);
    }
    switch (offerSdp->videoDirection) {
      case SENDONLY:
        this->videoDirection = RECVONLY;
        break;
      case RECVONLY:
        this->videoDirection = SENDONLY;
        break;
      case SENDRECV:
        this->videoDirection = SENDRECV;
        break;
      case INACTIVE:
        this->videoDirection = INACTIVE;
        break;
      default:
        this->videoDirection = SENDRECV;
        break;
    }
    switch (offerSdp->audioDirection) {
      case SENDONLY:
        this->audioDirection = RECVONLY;
        break;
      case RECVONLY:
        this->audioDirection = SENDONLY;
        break;
      case SENDRECV:
        this->audioDirection = SENDRECV;
        break;
      case INACTIVE:
        this->audioDirection = INACTIVE;
        break;
      default:
        this->audioDirection = SENDRECV;
        break;
    }
    ELOG_DEBUG("Offer SDP successfully set");
  }

  bool SdpInfo::postProcessInfo() {
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
        negotiated_map.channels = internal_map.channels;


        ELOG_DEBUG("message: Checking feedback types, parsed: %lu, internal:%lu", parsed_map.feedback_types.size(),
            internal_map.feedback_types.size());

        negotiated_map.feedback_types = negotiateFeedback(parsed_map, internal_map);
        negotiated_map.format_parameters = maybeCopyFormatParameters(parsed_map, internal_map);

        if (internal_map.format_parameters.empty() ||
            parsed_map.format_parameters.size() == negotiated_map.format_parameters.size()) {
          if (negotiated_map.media_type == VIDEO_TYPE) {
            videoCodecs++;
          } else {
            audioCodecs++;
          }
          ELOG_DEBUG("Mapping %s/%d:%d to %s/%d:%d",
              parsed_map.encoding_name.c_str(), parsed_map.clock_rate, parsed_map.payload_type,
              internal_map.encoding_name.c_str(), internal_map.clock_rate, internal_map.payload_type);
          payloadVector.push_back(negotiated_map);
          outInPTMap[parsed_map.payload_type] = internal_map.payload_type;
          inOutPTMap[internal_map.payload_type] = parsed_map.payload_type;
        }
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

  std::vector<std::string> SdpInfo::negotiateFeedback(const RtpMap& parsed_map,
      const RtpMap& internal_map) {
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
    return negotiated_feedback;
  }

  std::map<std::string, std::string> SdpInfo::maybeCopyFormatParameters(const RtpMap& parsed_map,
      const RtpMap& internal_map) {
    std::map<std::string, std::string> negotiated_format_parameters;
    if (parsed_map.format_parameters.size() == internal_map.format_parameters.size()) {
      for (const std::pair<std::string, std::string>& internal_parameter : internal_map.format_parameters) {
        auto found_parameter = parsed_map.format_parameters.find(internal_parameter.first);
        if (found_parameter != parsed_map.format_parameters.end()) {
          if (found_parameter->second == internal_parameter.second) {
            ELOG_DEBUG("message: Adding fmtp, codec_name: %s, parameter: %s, value:%s",
                parsed_map.encoding_name.c_str(), found_parameter->first.c_str(),
                found_parameter->second.c_str());
            negotiated_format_parameters[found_parameter->first] = found_parameter->second;
          }
        }
      }
    }
    return negotiated_format_parameters;
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

  std::vector<ExtMap> SdpInfo::getExtensionMap(MediaType media) {
    std::vector<ExtMap> valid_extensions;
    for (const ExtMap& extension : extMapVector) {
      if (isValidExtension(extension.uri) && extension.mediaType == media) {
        valid_extensions.push_back(extension);
      }
    }
    return valid_extensions;
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

  bool SdpInfo::processCandidate(const std::vector<std::string>& pieces, MediaType mediaType, std::string line) {
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
    cand.sdp = line;
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

  bool operator==(const Rid& lhs, const Rid& rhs) {
  return lhs.id == rhs.id && lhs.direction == rhs.direction;
  }

  std::ostream& operator<<(std::ostream& os, RidDirection dir) {
    switch (dir) {
      case RidDirection::SEND:
        os << "send";
    break;
      case RidDirection::RECV:
        os << "recv";
    break;
      default:
        assert(false);
    }
    return os;
  }

  RidDirection reverse(RidDirection dir) {
    switch (dir) {
      case RidDirection::SEND:
        return RidDirection::RECV;
      case RidDirection::RECV:
        return RidDirection::SEND;
      default:
        assert(false);
    }
    return dir;
  }

}  // namespace erizo
