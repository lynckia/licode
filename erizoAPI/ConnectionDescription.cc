#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "ConnectionDescription.h"

#include "lib/json.hpp"

using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Persistent;
using v8::Exception;
using v8::Value;
using json = nlohmann::json;

#define GET_SDP() \
  ConnectionDescription* obj = ObjectWrap::Unwrap<ConnectionDescription>(info.Holder()); \
  std::shared_ptr<erizo::SdpInfo> sdp = obj->me;

std::string getString(v8::Local<v8::Value> value) {
  Nan::Utf8String value_str(Nan::To<v8::String>(value).ToLocalChecked()); \
  return std::string(*value_str);
}

erizo::MediaType getMediaType(std::string media_type) {
  erizo::MediaType media_type_value;
  if (media_type == "audio") {
    media_type_value = erizo::AUDIO_TYPE;
  } else if (media_type == "video") {
    media_type_value = erizo::VIDEO_TYPE;
  } else {
    media_type_value = erizo::OTHER;
  }
  return media_type_value;
}

Nan::Persistent<Function> ConnectionDescription::constructor;

ConnectionDescription::ConnectionDescription() {
}

ConnectionDescription::~ConnectionDescription() {
}

NAN_MODULE_INIT(ConnectionDescription::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ConnectionDescription").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);

  Nan::SetPrototypeMethod(tpl, "setProfile", setProfile);
  Nan::SetPrototypeMethod(tpl, "setBundle", setBundle);
  Nan::SetPrototypeMethod(tpl, "addBundleTag", addBundleTag);
  Nan::SetPrototypeMethod(tpl, "setRtcpMux", setRtcpMux);
  Nan::SetPrototypeMethod(tpl, "setAudioAndVideo", setAudioAndVideo);

  Nan::SetPrototypeMethod(tpl, "getProfile", getProfile);
  Nan::SetPrototypeMethod(tpl, "isBundle", isBundle);
  Nan::SetPrototypeMethod(tpl, "getMediaId", getMediaId);
  Nan::SetPrototypeMethod(tpl, "isRtcpMux", isRtcpMux);
  Nan::SetPrototypeMethod(tpl, "hasAudio", hasAudio);
  Nan::SetPrototypeMethod(tpl, "hasVideo", hasVideo);

  Nan::SetPrototypeMethod(tpl, "setAudioSsrc", setAudioSsrc);
  Nan::SetPrototypeMethod(tpl, "getAudioSsrcMap", getAudioSsrcMap);
  Nan::SetPrototypeMethod(tpl, "setVideoSsrcList", setVideoSsrcList);
  Nan::SetPrototypeMethod(tpl, "getVideoSsrcMap", getVideoSsrcMap);

  Nan::SetPrototypeMethod(tpl, "setVideoDirection", setVideoDirection);
  Nan::SetPrototypeMethod(tpl, "setAudioDirection", setAudioDirection);
  Nan::SetPrototypeMethod(tpl, "getDirection", getDirection);

  Nan::SetPrototypeMethod(tpl, "setFingerprint", setFingerprint);
  Nan::SetPrototypeMethod(tpl, "getFingerprint", getFingerprint);
  Nan::SetPrototypeMethod(tpl, "setDtlsRole", setDtlsRole);
  Nan::SetPrototypeMethod(tpl, "getDtlsRole", getDtlsRole);

  Nan::SetPrototypeMethod(tpl, "setVideoBandwidth", setVideoBandwidth);
  Nan::SetPrototypeMethod(tpl, "getVideoBandwidth", getVideoBandwidth);

  Nan::SetPrototypeMethod(tpl, "setXGoogleFlag", setXGoogleFlag);
  Nan::SetPrototypeMethod(tpl, "getXGoogleFlag", getXGoogleFlag);

  Nan::SetPrototypeMethod(tpl, "addCandidate", addCandidate);
  Nan::SetPrototypeMethod(tpl, "addCryptoInfo", addCryptoInfo);
  Nan::SetPrototypeMethod(tpl, "setICECredentials", setICECredentials);
  Nan::SetPrototypeMethod(tpl, "getICECredentials", getICECredentials);

  Nan::SetPrototypeMethod(tpl, "getCandidates", getCandidates);
  Nan::SetPrototypeMethod(tpl, "getCodecs", getCodecs);
  Nan::SetPrototypeMethod(tpl, "getExtensions", getExtensions);

  Nan::SetPrototypeMethod(tpl, "addRid", addRid);
  Nan::SetPrototypeMethod(tpl, "addPt", addPt);
  Nan::SetPrototypeMethod(tpl, "addExtension", addExtension);
  Nan::SetPrototypeMethod(tpl, "addFeedback", addFeedback);
  Nan::SetPrototypeMethod(tpl, "addParameter", addParameter);

  Nan::SetPrototypeMethod(tpl, "getRids", getRids);

  Nan::SetPrototypeMethod(tpl, "postProcessInfo", postProcessInfo);
  Nan::SetPrototypeMethod(tpl, "copyInfoFromSdp", copyInfoFromSdp);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("ConnectionDescription").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}


v8::Local<v8::Object> ConnectionDescription::NewInstance() {
  v8::Local<v8::Function> cons = Nan::New(constructor);
  return Nan::NewInstance(cons).ToLocalChecked();
}

NAN_METHOD(ConnectionDescription::New) {
  if (info.Length() > 1) {
    Nan::ThrowError("Wrong number of arguments");
  }

  if (info.IsConstructCall()) {
    // Invoked as a constructor with 'new ConnectionDescription()'
    ConnectionDescription* obj = new ConnectionDescription();

    if (info.Length() == 0) {
      obj->Wrap(info.This());
      info.GetReturnValue().Set(info.This());
      return;
    }

    Nan::Utf8String json_param(Nan::To<v8::String>(info[0]).ToLocalChecked());
    std::string media_config_string = std::string(*json_param);
    json media_config = json::parse(media_config_string);

    std::vector<erizo::RtpMap> rtp_mappings;
    if (media_config.find("rtpMappings") != media_config.end()) {
      json rtp_map_json = media_config["rtpMappings"];
      for (json::iterator it = rtp_map_json.begin(); it != rtp_map_json.end(); ++it) {
        erizo::RtpMap rtp_map;
        if (it.value()["payloadType"].is_number()) {
          rtp_map.payload_type = it.value()["payloadType"];
        } else {
          continue;
        }
        if (it.value()["encodingName"].is_string()) {
          rtp_map.encoding_name = it.value()["encodingName"];
        } else {
          continue;
        }
        if (it.value()["mediaType"].is_string()) {
          if (it.value()["mediaType"] == "video") {
            rtp_map.media_type = erizo::VIDEO_TYPE;
          } else if (it.value()["mediaType"] == "audio") {
            rtp_map.media_type = erizo::AUDIO_TYPE;
          } else {
            continue;
          }
        } else {
          continue;
        }
        if (it.value()["clockRate"].is_number()) {
          rtp_map.clock_rate = it.value()["clockRate"];
        }
        if (rtp_map.media_type == erizo::AUDIO_TYPE) {
          if (it.value()["channels"].is_number()) {
            rtp_map.channels = it.value()["channels"];
          }
        }
        if (it.value()["formatParameters"].is_object()) {
          json format_params_json = it.value()["formatParameters"];
          for (json::iterator params_it = format_params_json.begin();
              params_it != format_params_json.end(); ++params_it) {
            std::string value = params_it.value();
            std::string key = params_it.key();
            rtp_map.format_parameters.insert(rtp_map.format_parameters.begin(),
                std::pair<std::string, std::string> (key, value));
          }
        }
        if (it.value()["feedbackTypes"].is_array()) {
          json feedback_types_json = it.value()["feedbackTypes"];
          for (json::iterator feedback_it = feedback_types_json.begin();
              feedback_it != feedback_types_json.end(); ++feedback_it) {
              rtp_map.feedback_types.push_back(*feedback_it);
          }
        }
        rtp_mappings.push_back(rtp_map);
      }
    }

    obj->me = std::make_shared<erizo::SdpInfo>(rtp_mappings);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // TODO(pedro) Check what happens here
  }
}

NAN_METHOD(ConnectionDescription::setProfile) {
  GET_SDP();
  std::string profile = getString(info[0]);

  if (profile == "SAVPF") {
    sdp->profile = erizo::SAVPF;
  } else if (profile == "AVPF") {
    sdp->profile = erizo::AVPF;
  }
}

NAN_METHOD(ConnectionDescription::setBundle) {
  GET_SDP();
  sdp->isBundle = Nan::To<bool>(info[0]).FromJust();
}

NAN_METHOD(ConnectionDescription::addBundleTag) {
  GET_SDP();
  std::string id = getString(info[0]);
  std::string media_type = getString(info[1]);
  sdp->bundleTags.push_back({id, getMediaType(media_type)});
}

NAN_METHOD(ConnectionDescription::setRtcpMux) {
  GET_SDP();
  sdp->isRtcpMux = Nan::To<bool>(info[0]).FromJust();
}

NAN_METHOD(ConnectionDescription::setAudioAndVideo) {
  GET_SDP();
  sdp->hasAudio = Nan::To<bool>(info[0]).FromJust();
  sdp->hasVideo = Nan::To<bool>(info[1]).FromJust();
}

NAN_METHOD(ConnectionDescription::getProfile) {
  GET_SDP();
  switch (sdp->profile) {
    case erizo::SAVPF:
     info.GetReturnValue().Set(Nan::New("UDP/TLS/RTP/SAVPF").ToLocalChecked());
     break;
    case erizo::AVPF:
    default:
      info.GetReturnValue().Set(Nan::New("RTP/AVPF").ToLocalChecked());
      break;
  }
}

NAN_METHOD(ConnectionDescription::isBundle) {
  GET_SDP();
  info.GetReturnValue().Set(Nan::New(sdp->isBundle));
}

NAN_METHOD(ConnectionDescription::getMediaId) {
  GET_SDP();
  erizo::MediaType media_type = getMediaType(getString(info[0]));
  std::string id = getString(info[0]);

  auto it = std::find_if(sdp->bundleTags.begin(), sdp->bundleTags.end(),
    [media_type](const erizo::BundleTag &tag) {
      return tag.mediaType == media_type;
    });

  if (it != sdp->bundleTags.end()) {
    id = it->id;
  }

  info.GetReturnValue().Set(Nan::New(id.c_str()).ToLocalChecked());
}

NAN_METHOD(ConnectionDescription::isRtcpMux) {
  GET_SDP();
  info.GetReturnValue().Set(Nan::New(sdp->isRtcpMux));
}

NAN_METHOD(ConnectionDescription::hasAudio) {
  GET_SDP();
  info.GetReturnValue().Set(Nan::New(sdp->hasAudio));
}

NAN_METHOD(ConnectionDescription::hasVideo) {
  GET_SDP();
  info.GetReturnValue().Set(Nan::New(sdp->hasVideo));
}

NAN_METHOD(ConnectionDescription::setAudioSsrc) {
  GET_SDP();
  std::string stream_id = getString(info[0]);
  sdp->audio_ssrc_map[stream_id] = Nan::To<unsigned int>(info[1]).FromJust();
}

NAN_METHOD(ConnectionDescription::getAudioSsrcMap) {
  GET_SDP();
  Local<v8::Object> audio_ssrc_map = Nan::New<v8::Object>();
  for (auto const& audio_ssrcs : sdp->audio_ssrc_map) {
    Nan::Set(audio_ssrc_map, Nan::New(audio_ssrcs.first.c_str()).ToLocalChecked(),
                        Nan::New(audio_ssrcs.second));
  }
  info.GetReturnValue().Set(audio_ssrc_map);
}

NAN_METHOD(ConnectionDescription::setVideoSsrcList) {
  GET_SDP();
  std::string stream_id = getString(info[0]);
  v8::Local<v8::Array> video_ssrc_array = v8::Local<v8::Array>::Cast(info[1]);
  std::vector<uint32_t> video_ssrc_list;

  for (unsigned int i = 0; i < video_ssrc_array->Length(); i++) {
    v8::Local<v8::Value> val = Nan::Get(video_ssrc_array, i).ToLocalChecked();
    unsigned int numVal = Nan::To<unsigned int>(val).FromJust();
    video_ssrc_list.push_back(numVal);
  }

  sdp->video_ssrc_map[stream_id] = video_ssrc_list;
}

NAN_METHOD(ConnectionDescription::getVideoSsrcMap) {
  GET_SDP();
  Local<v8::Object> video_ssrc_map = Nan::New<v8::Object>();
  for (auto const& video_ssrcs : sdp->video_ssrc_map) {
    v8::Local<v8::Array> array = Nan::New<v8::Array>(video_ssrcs.second.size());
    uint index = 0;
    for (uint32_t ssrc : video_ssrcs.second) {
      Nan::Set(array, index++, Nan::New(ssrc));
    }
    Nan::Set(video_ssrc_map, Nan::New(video_ssrcs.first.c_str()).ToLocalChecked(), array);
  }
  info.GetReturnValue().Set(video_ssrc_map);
}

NAN_METHOD(ConnectionDescription::setVideoDirection) {
  GET_SDP();
  std::string direction = getString(info[0]);

  if (direction == "sendonly") {
    sdp->videoDirection = erizo::SENDONLY;
  } else if (direction == "sendrecv") {
    sdp->videoDirection = erizo::SENDRECV;
  } else if (direction == "recvonly") {
    sdp->videoDirection = erizo::RECVONLY;
  } else if (direction == "inactive") {
    sdp->videoDirection = erizo::INACTIVE;
  }
}

NAN_METHOD(ConnectionDescription::setAudioDirection) {
  GET_SDP();
  std::string direction = getString(info[0]);

  if (direction == "sendonly") {
    sdp->audioDirection = erizo::SENDONLY;
  } else if (direction == "sendrecv") {
    sdp->audioDirection = erizo::SENDRECV;
  } else if (direction == "recvonly") {
    sdp->audioDirection = erizo::RECVONLY;
  } else if (direction == "inactive") {
    sdp->audioDirection = erizo::INACTIVE;
  }
}

NAN_METHOD(ConnectionDescription::getDirection) {
  GET_SDP();
  std::string media = getString(info[0]);
  erizo::StreamDirection direction;
  if (media == "audio") {
    direction = sdp->audioDirection;
  } else {
    direction = sdp->videoDirection;
  }
  std::string value = "";
  switch (direction) {
    case erizo::SENDONLY:
      value = "sendonly";
      break;
    case erizo::SENDRECV:
      value = "sendrecv";
      break;
    case erizo::INACTIVE:
      value = "inactive";
      break;
    case erizo::RECVONLY:
    default:
      value = "recvonly";
      break;
  }
  info.GetReturnValue().Set(Nan::New(value.c_str()).ToLocalChecked());
}

NAN_METHOD(ConnectionDescription::setFingerprint) {
  GET_SDP();
  sdp->fingerprint = getString(info[0]);
  sdp->isFingerprint = true;
}

NAN_METHOD(ConnectionDescription::getFingerprint) {
  GET_SDP();
  info.GetReturnValue().Set(Nan::New(sdp->fingerprint.c_str()).ToLocalChecked());
}

NAN_METHOD(ConnectionDescription::setDtlsRole) {
  GET_SDP();
  std::string dtls_role = getString(info[0]);

  if (dtls_role == "actpass") {
    sdp->dtlsRole = erizo::ACTPASS;
  } else if (dtls_role == "passive") {
    sdp->dtlsRole = erizo::PASSIVE;
  } else if (dtls_role == "active") {
    sdp->dtlsRole = erizo::ACTIVE;
  }
}

NAN_METHOD(ConnectionDescription::getDtlsRole) {
  GET_SDP();
  std::string dtls_role = "";
  switch (sdp->dtlsRole) {
    case erizo::ACTPASS:
      dtls_role = "actpass";
      break;
    case erizo::PASSIVE:
      dtls_role = "passive";
      break;
    case erizo::ACTIVE:
    default:
      dtls_role = "active";
      break;
  }
  info.GetReturnValue().Set(Nan::New(dtls_role.c_str()).ToLocalChecked());
}

NAN_METHOD(ConnectionDescription::setVideoBandwidth) {
  GET_SDP();
  sdp->videoBandwidth = Nan::To<unsigned int>(info[0]).FromJust();
}

NAN_METHOD(ConnectionDescription::getVideoBandwidth) {
  GET_SDP();
  info.GetReturnValue().Set(Nan::New(sdp->videoBandwidth));
}

NAN_METHOD(ConnectionDescription::setXGoogleFlag) {
  GET_SDP();
  sdp->google_conference_flag_set = getString(info[0]);
}

NAN_METHOD(ConnectionDescription::getXGoogleFlag) {
  GET_SDP();
  info.GetReturnValue().Set(Nan::New(sdp->google_conference_flag_set.c_str()).ToLocalChecked());
}

NAN_METHOD(ConnectionDescription::addCandidate) {
  GET_SDP();
  erizo::CandidateInfo cand;

  cand.mediaType = getMediaType(getString(info[0]));
  cand.foundation = getString(info[1]);
  cand.componentId = Nan::To<unsigned int>(info[2]).FromJust();
  cand.netProtocol = getString(info[3]);
  cand.priority = Nan::To<unsigned int>(info[4]).FromJust();
  cand.hostAddress = getString(info[5]);
  cand.hostPort = Nan::To<unsigned int>(info[6]).FromJust();

  // libnice does not support tcp candidates, we ignore them
  if (cand.netProtocol.compare("UDP") && cand.netProtocol.compare("udp")) {
    info.GetReturnValue().Set(Nan::New(false));
    return;
  }

  std::string type = getString(info[7]);
  if (type == "host") {
    cand.hostType = erizo::HOST;
  } else if (type == "srflx") {
    cand.hostType = erizo::SRFLX;
  } else if (type == "prflx") {
    cand.hostType = erizo::PRFLX;
  } else if (type == "relay") {
    cand.hostType = erizo::RELAY;
  } else {
    cand.hostType = erizo::HOST;
  }

  if (cand.hostType == erizo::SRFLX || cand.hostType == erizo::RELAY) {
    cand.rAddress = getString(info[8]);
    cand.rPort = Nan::To<unsigned int>(info[9]).FromJust();
  }

  cand.sdp = getString(info[10]);

  sdp->candidateVector_.push_back(cand);
  info.GetReturnValue().Set(Nan::New(true));
}

NAN_METHOD(ConnectionDescription::addCryptoInfo) {
  GET_SDP();
  erizo::CryptoInfo crypinfo;

  crypinfo.cipherSuite = getString(info[0]);
  crypinfo.keyParams = getString(info[1]);;
  crypinfo.mediaType = getMediaType(getString(info[2]));
  sdp->cryptoVector_.push_back(crypinfo);
}

NAN_METHOD(ConnectionDescription::setICECredentials) {
  GET_SDP();
  std::string username = getString(info[0]);
  std::string password = getString(info[1]);
  erizo::MediaType media = getMediaType(getString(info[2]));
  switch (media) {
    case(erizo::VIDEO_TYPE):
      sdp->iceVideoUsername_ = std::string(username);
      sdp->iceVideoPassword_ = std::string(password);
      break;
    case(erizo::AUDIO_TYPE):
      sdp->iceAudioUsername_ = std::string(username);
      sdp->iceAudioPassword_ = std::string(password);
      break;
    default:
      sdp->iceVideoUsername_ = std::string(username);
      sdp->iceVideoPassword_ = std::string(password);
      sdp->iceAudioUsername_ = std::string(username);
      sdp->iceAudioPassword_ = std::string(password);
      break;
  }
}

NAN_METHOD(ConnectionDescription::getICECredentials) {
  GET_SDP();
  erizo::MediaType media = getMediaType(getString(info[0]));
  std::string username = "";
  std::string password = "";
  switch (media) {
    case erizo::VIDEO_TYPE:
      username = sdp->iceVideoUsername_;
      password = sdp->iceVideoPassword_;
      break;
    case erizo::AUDIO_TYPE:
    default:
      username = sdp->iceAudioUsername_;
      password = sdp->iceAudioPassword_;
      break;
  }
  v8::Local<v8::Array> array = Nan::New<v8::Array>(2);
  Nan::Set(array, 0, Nan::New(username.c_str()).ToLocalChecked());
  Nan::Set(array, 1, Nan::New(password.c_str()).ToLocalChecked());

  info.GetReturnValue().Set(array);
}

NAN_METHOD(ConnectionDescription::getCandidates) {
  GET_SDP();
  std::vector<erizo::CandidateInfo>& candidates = sdp->getCandidateInfos();

  v8::Local<v8::Array> candidates_array = Nan::New<v8::Array>(candidates.size());
  uint index = 0;
  for (erizo::CandidateInfo &candidate : candidates) {
    v8::Local<v8::Object> candidate_info = Nan::New<v8::Object>();
    Nan::Set(candidate_info, Nan::New("bundle").ToLocalChecked(), Nan::New(candidate.isBundle));
    Nan::Set(candidate_info, Nan::New("tag").ToLocalChecked(), Nan::New(candidate.tag));
    Nan::Set(candidate_info, Nan::New("priority").ToLocalChecked(), Nan::New(candidate.priority));
    Nan::Set(candidate_info, Nan::New("componentId").ToLocalChecked(), Nan::New(candidate.componentId));
    Nan::Set(candidate_info, Nan::New("foundation").ToLocalChecked(),
                                        Nan::New(candidate.foundation.c_str()).ToLocalChecked());
    Nan::Set(candidate_info, Nan::New("hostIp").ToLocalChecked(),
                                        Nan::New(candidate.hostAddress.c_str()).ToLocalChecked());
    Nan::Set(candidate_info, Nan::New("relayIp").ToLocalChecked(),
                                        Nan::New(candidate.rAddress.c_str()).ToLocalChecked());
    Nan::Set(candidate_info, Nan::New("hostPort").ToLocalChecked(), Nan::New(candidate.hostPort));
    Nan::Set(candidate_info, Nan::New("relayPort").ToLocalChecked(), Nan::New(candidate.rPort));
    Nan::Set(candidate_info, Nan::New("protocol").ToLocalChecked(),
                                        Nan::New(candidate.netProtocol.c_str()).ToLocalChecked());
    std::string host_type = "host";
    switch (candidate.hostType) {
      case erizo::HOST:
        host_type = "host";
        break;
      case erizo::SRFLX:
        host_type = "srflx";
        break;
      case erizo::PRFLX:
        host_type = "prflx";
        break;
      case erizo::RELAY:
      default:
        host_type = "relay";
        break;
    }
    Nan::Set(candidate_info, Nan::New("hostType").ToLocalChecked(),
                                        Nan::New(host_type.c_str()).ToLocalChecked());
    Nan::Set(candidate_info, Nan::New("transport").ToLocalChecked(),
                                        Nan::New(candidate.transProtocol.c_str()).ToLocalChecked());
    Nan::Set(candidate_info, Nan::New("user").ToLocalChecked(),
                                        Nan::New(candidate.username.c_str()).ToLocalChecked());
    Nan::Set(candidate_info, Nan::New("pass").ToLocalChecked(),
                                        Nan::New(candidate.password.c_str()).ToLocalChecked());
    std::string media_type = "audio";
    switch (candidate.mediaType) {
      case erizo::VIDEO_TYPE:
        media_type = "video";
        break;
      case erizo::AUDIO_TYPE:
      default:
        media_type = "audio";
        break;
    }
    Nan::Set(candidate_info, Nan::New("mediaType").ToLocalChecked(),
                                        Nan::New(media_type.c_str()).ToLocalChecked());

    Nan::Set(candidates_array, index++, candidate_info);
  }
  info.GetReturnValue().Set(candidates_array);
}

NAN_METHOD(ConnectionDescription::getCodecs) {
  GET_SDP();
  std::string media_type_str = getString(info[0]);
  erizo::MediaType media_type = getMediaType(media_type_str);
  std::vector<erizo::RtpMap> pts = sdp->getPayloadInfos();
  std::vector<erizo::RtpMap> media_pts(pts.size());
  auto it = std::copy_if(pts.begin(), pts.end(), media_pts.begin(), [media_type](const erizo::RtpMap &pt) {
    return pt.media_type == media_type;
  });
  media_pts.resize(std::distance(media_pts.begin(), it));
  v8::Local<v8::Array> codecs = Nan::New<v8::Array>(media_pts.size());
  uint index = 0;
  for (erizo::RtpMap &pt : media_pts) {
    v8::Local<v8::Object> codec = Nan::New<v8::Object>();
    Nan::Set(codec, Nan::New("type").ToLocalChecked(), Nan::New(pt.payload_type));
    Nan::Set(codec, Nan::New("name").ToLocalChecked(),
                                Nan::New(pt.encoding_name.c_str()).ToLocalChecked());
    Nan::Set(codec, Nan::New("rate").ToLocalChecked(), Nan::New(pt.clock_rate));
    Nan::Set(codec, Nan::New("mediaType").ToLocalChecked(),
                                Nan::New(media_type_str.c_str()).ToLocalChecked());
    Nan::Set(codec, Nan::New("channels").ToLocalChecked(), Nan::New(pt.channels));

    std::vector<std::string> &feedback_types = pt.feedback_types;
    v8::Local<v8::Array> feedbacks = Nan::New<v8::Array>(feedback_types.size());
    uint fb_index = 0;
    for (std::string feedback_type : feedback_types) {
      Nan::Set(feedbacks, fb_index++, Nan::New(feedback_type.c_str()).ToLocalChecked());
    }
    Nan::Set(codec, Nan::New("feedbacks").ToLocalChecked(), feedbacks);

    Local<v8::Object> parameters = Nan::New<v8::Object>();
    std::map<std::string, std::string> &params = pt.format_parameters;
    for (auto const& param : params) {
      Nan::Set(parameters, Nan::New(param.first).ToLocalChecked(), Nan::New(param.second).ToLocalChecked());
    }
    Nan::Set(codec, Nan::New("params").ToLocalChecked(), parameters);

    Nan::Set(codecs, index++, codec);
  }
  info.GetReturnValue().Set(codecs);
}

NAN_METHOD(ConnectionDescription::getExtensions) {
  GET_SDP();
  std::string media_type_str = getString(info[0]);
  erizo::MediaType media_type = getMediaType(media_type_str);
  std::vector<erizo::ExtMap> media_extensions = sdp->getExtensionMap(media_type);

  Local<v8::Object> extensions = Nan::New<v8::Object>();
  for (erizo::ExtMap& extension : media_extensions) {
    Nan::Set(extensions, Nan::New(extension.value), Nan::New(extension.uri).ToLocalChecked());
  }
  info.GetReturnValue().Set(extensions);
}

NAN_METHOD(ConnectionDescription::addRid) {
  GET_SDP();

  std::string id = getString(info[0]);
  std::string direction = getString(info[1]);

  erizo::RidDirection rid_direction = erizo::RidDirection::SEND;

  if (direction == "send") {
    rid_direction = erizo::RidDirection::SEND;
  } else if (direction == "recv") {
    rid_direction = erizo::RidDirection::RECV;
  }

  sdp->rids_.push_back({id, rid_direction});
}

NAN_METHOD(ConnectionDescription::addPt) {
  GET_SDP();
  unsigned int pt = Nan::To<unsigned int>(info[0]).FromJust();
  std::string codec_name = getString(info[1]);
  unsigned int parsed_clock = Nan::To<unsigned int>(info[2]).FromJust();
  erizo::MediaType media = getMediaType(getString(info[3]));

  erizo::RtpMap new_mapping;
  new_mapping.payload_type = pt;
  new_mapping.encoding_name = codec_name;
  new_mapping.clock_rate = parsed_clock;
  new_mapping.media_type = media;
  sdp->payload_parsed_map_[pt] = new_mapping;
}

NAN_METHOD(ConnectionDescription::addExtension) {
  GET_SDP();
  unsigned int id = Nan::To<unsigned int>(info[0]).FromJust();
  std::string uri = getString(info[1]);
  erizo::MediaType media = getMediaType(getString(info[2]));
  erizo::ExtMap anExt(id, uri);
  anExt.mediaType = media;
  sdp->extMapVector.push_back(anExt);
}

NAN_METHOD(ConnectionDescription::addFeedback) {
  GET_SDP();
  unsigned int pt = Nan::To<unsigned int>(info[0]).FromJust();
  std::string feedback = getString(info[1]);
  auto map_element = sdp->payload_parsed_map_.find(pt);
  if (map_element != sdp->payload_parsed_map_.end()) {
    map_element->second.feedback_types.push_back(feedback);
  } else {
    erizo::RtpMap new_map;
    new_map.payload_type = pt;
    new_map.feedback_types.push_back(feedback);
    sdp->payload_parsed_map_[pt] = new_map;
  }
}

NAN_METHOD(ConnectionDescription::addParameter) {
  GET_SDP();
  unsigned int pt = Nan::To<unsigned int>(info[0]).FromJust();
  std::string option = getString(info[1]);
  std::string value = getString(info[2]);
  auto map_element = sdp->payload_parsed_map_.find(pt);
  if (map_element != sdp->payload_parsed_map_.end()) {
    map_element->second.format_parameters[option] = value;
  } else {
    erizo::RtpMap new_map;
    new_map.payload_type = pt;
    new_map.format_parameters[option] = value;
    sdp->payload_parsed_map_[pt] = new_map;
  }
}

NAN_METHOD(ConnectionDescription::getRids) {
  GET_SDP();
  v8::Local<v8::Object> rids = Nan::New<v8::Object>();
  for (const erizo::Rid& rid : sdp->rids()) {
    std::ostringstream direction;
    direction << rid.direction;
    Nan::Set(rids, Nan::New(rid.id).ToLocalChecked(), Nan::New(direction.str()).ToLocalChecked());
  }
  info.GetReturnValue().Set(rids);
}

NAN_METHOD(ConnectionDescription::postProcessInfo) {
  GET_SDP();
  bool success = sdp->postProcessInfo();
  info.GetReturnValue().Set(Nan::New(success));
}

NAN_METHOD(ConnectionDescription::copyInfoFromSdp) {
  GET_SDP();
  ConnectionDescription* source =
    Nan::ObjectWrap::Unwrap<ConnectionDescription>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

  std::shared_ptr<erizo::SdpInfo> source_sdp = source->me;
  sdp->copyInfoFromSdp(source_sdp);
}

NAN_METHOD(ConnectionDescription::close) {
  GET_SDP();
  obj->me.reset();
}
