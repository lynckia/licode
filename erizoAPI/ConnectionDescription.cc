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
  v8::String::Utf8Value value_str(Nan::To<v8::String>(value).ToLocalChecked()); \
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

  Nan::SetPrototypeMethod(tpl, "setAudioSsrc", setAudioSsrc);
  Nan::SetPrototypeMethod(tpl, "setVideoSsrcList", setVideoSsrcList);

  Nan::SetPrototypeMethod(tpl, "setDirection", setDirection);

  Nan::SetPrototypeMethod(tpl, "setFingerprint", setFingerprint);
  Nan::SetPrototypeMethod(tpl, "setDtlsRole", setDtlsRole);

  Nan::SetPrototypeMethod(tpl, "setVideoBandwidth", setVideoBandwidth);

  Nan::SetPrototypeMethod(tpl, "addCandidate", addCandidate);
  Nan::SetPrototypeMethod(tpl, "addCryptoInfo", addCryptoInfo);
  Nan::SetPrototypeMethod(tpl, "setICECredentials", setICECredentials);

  Nan::SetPrototypeMethod(tpl, "addRid", addRid);
  Nan::SetPrototypeMethod(tpl, "addPt", addPt);
  Nan::SetPrototypeMethod(tpl, "addExtension", addExtension);
  Nan::SetPrototypeMethod(tpl, "addFeedback", addFeedback);
  Nan::SetPrototypeMethod(tpl, "addParameter", addParameter);

  Nan::SetPrototypeMethod(tpl, "postProcessInfo", postProcessInfo);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("ConnectionDescription").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}


NAN_METHOD(ConnectionDescription::New) {
  if (info.Length() != 1) {
    Nan::ThrowError("Wrong number of arguments");
  }

  if (info.IsConstructCall()) {
    // Invoked as a constructor with 'new ConnectionDescription()'
    ConnectionDescription* obj = new ConnectionDescription();

    v8::String::Utf8Value json_param(Nan::To<v8::String>(info[0]).ToLocalChecked());
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
  sdp->isBundle = info[0]->BooleanValue();
}

NAN_METHOD(ConnectionDescription::addBundleTag) {
  GET_SDP();
  std::string id = getString(info[0]);
  std::string media_type = getString(info[1]);
  sdp->bundleTags.push_back({id, getMediaType(media_type)});
}

NAN_METHOD(ConnectionDescription::setRtcpMux) {
  GET_SDP();
  sdp->isRtcpMux = info[0]->BooleanValue();
}

NAN_METHOD(ConnectionDescription::setAudioAndVideo) {
  GET_SDP();
  sdp->hasAudio = info[0]->BooleanValue();
  sdp->hasVideo = info[1]->BooleanValue();
}

NAN_METHOD(ConnectionDescription::setAudioSsrc) {
  GET_SDP();
  sdp->audio_ssrc = info[0]->IntegerValue();
}

NAN_METHOD(ConnectionDescription::setVideoSsrcList) {
  GET_SDP();
  v8::Local<v8::Array> video_ssrc_array = v8::Local<v8::Array>::Cast(info[0]);
  std::vector<uint32_t> video_ssrc_list;

  for (unsigned int i = 0; i < video_ssrc_array->Length(); i++) {
    v8::Handle<v8::Value> val = video_ssrc_array->Get(i);
    unsigned int numVal = val->IntegerValue();
    video_ssrc_list.push_back(numVal);
  }

  sdp->video_ssrc_list = video_ssrc_list;
}

NAN_METHOD(ConnectionDescription::setDirection) {
  GET_SDP();
  std::string direction = getString(info[0]);

  if (direction == "sendonly") {
    sdp->audioDirection = erizo::SENDONLY;
    sdp->videoDirection = erizo::SENDONLY;
  } else if (direction == "sendrecv") {
    sdp->audioDirection = erizo::SENDRECV;
    sdp->videoDirection = erizo::SENDRECV;
  } else if (direction == "recvonly") {
    sdp->audioDirection = erizo::RECVONLY;
    sdp->videoDirection = erizo::RECVONLY;
  }
}

NAN_METHOD(ConnectionDescription::setFingerprint) {
  GET_SDP();
  sdp->fingerprint = getString(info[0]);
  sdp->isFingerprint = true;
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

NAN_METHOD(ConnectionDescription::setVideoBandwidth) {
  GET_SDP();
  sdp->videoBandwidth = info[0]->IntegerValue();
}

NAN_METHOD(ConnectionDescription::addCandidate) {
  GET_SDP();
  erizo::CandidateInfo cand;

  cand.mediaType = getMediaType(getString(info[0]));
  cand.foundation = getString(info[1]);
  cand.componentId = info[2]->IntegerValue();
  cand.netProtocol = getString(info[3]);
  cand.priority = info[4]->IntegerValue();
  cand.hostAddress = getString(info[5]);
  cand.hostPort = info[6]->IntegerValue();

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
    cand.rPort = info[9]->IntegerValue();
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
  unsigned int pt = info[0]->IntegerValue();
  std::string codec_name = getString(info[1]);
  unsigned int parsed_clock = info[2]->IntegerValue();
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
  unsigned int id = info[0]->IntegerValue();
  std::string uri = getString(info[1]);
  erizo::MediaType media = getMediaType(getString(info[2]));
  erizo::ExtMap anExt(id, uri);
  anExt.mediaType = media;
  sdp->extMapVector.push_back(anExt);
}

NAN_METHOD(ConnectionDescription::addFeedback) {
  GET_SDP();
  unsigned int pt = info[0]->IntegerValue();
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
  unsigned int pt = info[0]->IntegerValue();
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

NAN_METHOD(ConnectionDescription::postProcessInfo) {
  GET_SDP();
  bool success = sdp->postProcessInfo();
  info.GetReturnValue().Set(Nan::New(success));
}

NAN_METHOD(ConnectionDescription::close) {
  GET_SDP();
  obj->me.reset();
}
