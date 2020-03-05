#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "WebRtcConnection.h"
#include "ConnectionDescription.h"
#include "MediaStream.h"

#include <future>  // NOLINT
#include <boost/thread/future.hpp>  // NOLINT
#include <boost/thread/mutex.hpp>  // NOLINT

#include "lib/json.hpp"
#include "IOThreadPool.h"
#include "ThreadPool.h"

using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Persistent;
using v8::Exception;
using v8::Value;
using json = nlohmann::json;
using erizo::CandidateInfo;
using erizo::HostType;

Nan::Persistent<Function> WebRtcConnection::constructor;

DEFINE_LOGGER(WebRtcConnection, "ErizoAPI.WebRtcConnection");

ConnectionStatCallWorker::ConnectionStatCallWorker(
  Nan::Callback *callback, std::weak_ptr<erizo::WebRtcConnection> weak_connection)
    : Nan::AsyncWorker{callback}, weak_connection_{weak_connection}, stat_{""} {
}

void ConnectionStatCallWorker::Execute() {
  std::promise<std::string> stat_promise;
  std::future<std::string> stat_future = stat_promise.get_future();
  if (auto connection = weak_connection_.lock()) {
    connection->getJSONStats([&stat_promise] (std::string stats) {
      stat_promise.set_value(stats);
    });
  } else {
    stat_promise.set_value(std::string("{}"));
  }
  stat_future.wait();
  stat_ = stat_future.get();
}

void ConnectionStatCallWorker::HandleOKCallback() {
  Local<Value> argv[] = {
    Nan::New<v8::String>(stat_).ToLocalChecked()
  };
  Nan::AsyncResource resource("erizo::addon.statCall");
  callback->Call(1, argv, &resource);
}

void destroyWebRtcConnectionAsyncHandle(uv_handle_t *handle) {
  delete handle;
}

WebRtcConnection::WebRtcConnection() : closed_{false}, id_{"undefined"} {
  async_ = new uv_async_t;
  future_async_ = new uv_async_t;
  uv_async_init(uv_default_loop(), async_, &WebRtcConnection::eventsCallback);
  uv_async_init(uv_default_loop(), future_async_, &WebRtcConnection::promiseResolver);
}

WebRtcConnection::~WebRtcConnection() {
  close();
  ELOG_DEBUG("%s, message: Destroyed", toLog());
}

void WebRtcConnection::close() {
  ELOG_DEBUG("%s, message: Trying to close", toLog());
  if (closed_) {
    ELOG_DEBUG("%s, message: Already closed", toLog());
    return;
  }
  ELOG_DEBUG("%s, message: Closing", toLog());
  if (me) {
    me->setWebRtcConnectionEventListener(nullptr);
    me->close();
    me.reset();
  }

  boost::mutex::scoped_lock lock(mutex);

  if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(async_))) {
    ELOG_DEBUG("%s, message: Closing handle", toLog());
    uv_close(reinterpret_cast<uv_handle_t*>(async_), destroyWebRtcConnectionAsyncHandle);
  }

  if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(future_async_))) {
    ELOG_DEBUG("%s, message: Closing future handle", toLog());
    uv_close(reinterpret_cast<uv_handle_t*>(future_async_), destroyWebRtcConnectionAsyncHandle);
  }
  async_ = nullptr;
  future_async_ = nullptr;
  closed_ = true;
  ELOG_DEBUG("%s, message: Closed", toLog());
}

std::string WebRtcConnection::toLog() {
  return "id:" + id_;
}

NAN_MODULE_INIT(WebRtcConnection::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("WebRtcConnection").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);
  Nan::SetPrototypeMethod(tpl, "setRemoteDescription", setRemoteDescription);
  Nan::SetPrototypeMethod(tpl, "getLocalDescription", getLocalDescription);
  Nan::SetPrototypeMethod(tpl, "addRemoteCandidate", addRemoteCandidate);
  Nan::SetPrototypeMethod(tpl, "getCurrentState", getCurrentState);
  Nan::SetPrototypeMethod(tpl, "getConnectionQualityLevel", getConnectionQualityLevel);
  Nan::SetPrototypeMethod(tpl, "createOffer", createOffer);
  Nan::SetPrototypeMethod(tpl, "setMetadata", setMetadata);
  Nan::SetPrototypeMethod(tpl, "addMediaStream", addMediaStream);
  Nan::SetPrototypeMethod(tpl, "removeMediaStream", removeMediaStream);
  Nan::SetPrototypeMethod(tpl, "copySdpToLocalDescription", copySdpToLocalDescription);
  Nan::SetPrototypeMethod(tpl, "getStats", getStats);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("WebRtcConnection").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}


NAN_METHOD(WebRtcConnection::New) {
  if (info.Length() < 7) {
    Nan::ThrowError("Wrong number of arguments");
  }

  if (info.IsConstructCall()) {
    // Invoked as a constructor with 'new WebRTC()'
    ThreadPool* thread_pool = Nan::ObjectWrap::Unwrap<ThreadPool>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
    IOThreadPool* io_thread_pool = Nan::ObjectWrap::Unwrap<IOThreadPool>(Nan::To<v8::Object>(info[1]).ToLocalChecked());
    Nan::Utf8String paramId(Nan::To<v8::String>(info[2]).ToLocalChecked());
    std::string wrtcId = std::string(*paramId);
    Nan::Utf8String param(Nan::To<v8::String>(info[3]).ToLocalChecked());
    std::string stunServer = std::string(*param);
    int stunPort = Nan::To<int>(info[4]).FromJust();
    int minPort = Nan::To<int>(info[5]).FromJust();
    int maxPort = Nan::To<int>(info[6]).FromJust();
    bool trickle = Nan::To<bool>((info[7])).FromJust();
    Nan::Utf8String json_param(Nan::To<v8::String>(info[8]).ToLocalChecked());
    bool enable_connection_quality_check = Nan::To<bool>((info[9])).FromJust();
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

    std::vector<erizo::ExtMap> ext_mappings;
    unsigned int value = 0;
    if (media_config.find("extMappings") != media_config.end()) {
      json ext_map_json = media_config["extMappings"];
      for (json::iterator ext_map_it = ext_map_json.begin(); ext_map_it != ext_map_json.end(); ++ext_map_it) {
        ext_mappings.push_back({value++, *ext_map_it});
      }
    }

    erizo::IceConfig iceConfig;
    if (info.Length() == 15) {
      Nan::Utf8String param2(Nan::To<v8::String>(info[10]).ToLocalChecked());
      std::string turnServer = std::string(*param2);
      int turnPort = Nan::To<int>(info[11]).FromJust();
      Nan::Utf8String param3(Nan::To<v8::String>(info[12]).ToLocalChecked());
      std::string turnUsername = std::string(*param3);
      Nan::Utf8String param4(Nan::To<v8::String>(info[13]).ToLocalChecked());
      std::string turnPass = std::string(*param4);
      Nan::Utf8String param5(Nan::To<v8::String>(info[14]).ToLocalChecked());
      std::string network_interface = std::string(*param5);

      iceConfig.turn_server = turnServer;
      iceConfig.turn_port = turnPort;
      iceConfig.turn_username = turnUsername;
      iceConfig.turn_pass = turnPass;
      iceConfig.network_interface = network_interface;
    }


    iceConfig.stun_server = stunServer;
    iceConfig.stun_port = stunPort;
    iceConfig.min_port = minPort;
    iceConfig.max_port = maxPort;
    iceConfig.should_trickle = trickle;

    std::shared_ptr<erizo::Worker> worker = thread_pool->me->getLessUsedWorker();
    std::shared_ptr<erizo::IOWorker> io_worker = io_thread_pool->me->getLessUsedIOWorker();

    WebRtcConnection* obj = new WebRtcConnection();
    obj->id_ = wrtcId;
    obj->me = std::make_shared<erizo::WebRtcConnection>(worker, io_worker, wrtcId, iceConfig,
                                                        rtp_mappings, ext_mappings, enable_connection_quality_check,
                                                        obj);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // TODO(pedro) Check what happens here
  }
}

NAN_METHOD(WebRtcConnection::close) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  obj->close();
}

NAN_METHOD(WebRtcConnection::init) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }

  obj->event_callback_ = new Nan::Callback(info[0].As<Function>());
  bool r = me->init();

  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(WebRtcConnection::createOffer) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }

  if (info.Length() < 3) {
    Nan::ThrowError("Wrong number of arguments");
  }
  bool video_enabled = Nan::To<bool>(info[0]).FromJust();
  bool audio_enabled = Nan::To<bool>(info[1]).FromJust();
  bool bundle = Nan::To<bool>(info[2]).FromJust();

  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(Nan::GetCurrentContext()).ToLocalChecked();
  Nan::Persistent<v8::Promise::Resolver> *persistent = new Nan::Persistent<v8::Promise::Resolver>(resolver);
  obj->Ref();
  me->createOffer(video_enabled, audio_enabled, bundle).then(
    [persistent, obj] (boost::future<void>) {
      obj->notifyFuture(persistent);
    });

  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_METHOD(WebRtcConnection::setMetadata) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }

  Nan::Utf8String json_param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string metadata_string = std::string(*json_param);
  json metadata_json = json::parse(metadata_string);
  std::map<std::string, std::string> metadata;
  for (json::iterator item = metadata_json.begin(); item != metadata_json.end(); ++item) {
    std::string value = item.value().dump();
    if (item.value().is_object()) {
      value = "[object]";
    }
    if (item.value().is_string()) {
      value = item.value();
    }
    metadata[item.key()] = value;
  }

  me->setMetadata(metadata);

  info.GetReturnValue().Set(Nan::New(true));
}

NAN_METHOD(WebRtcConnection::setRemoteDescription) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    info.GetReturnValue().Set(Nan::New(false));
    return;
  }

  ConnectionDescription* param =
    Nan::ObjectWrap::Unwrap<ConnectionDescription>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  int received_session_version = Nan::To<int>(info[1]).FromJust();
  auto sdp = std::make_shared<erizo::SdpInfo>(*param->me.get());

  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(Nan::GetCurrentContext()).ToLocalChecked();
  Nan::Persistent<v8::Promise::Resolver> *persistent = new Nan::Persistent<v8::Promise::Resolver>(resolver);

  obj->Ref();
  me->setRemoteSdpInfo(sdp, received_session_version).then(
    [persistent, obj] (boost::future<void>) {
      obj->notifyFuture(persistent);
    });

  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_METHOD(WebRtcConnection::getLocalDescription) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }
  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(Nan::GetCurrentContext()).ToLocalChecked();
  Nan::Persistent<v8::Promise::Resolver> *persistent = new Nan::Persistent<v8::Promise::Resolver>(resolver);
  obj->Ref();

  me->getLocalSdpInfo().then(
      [persistent, obj] (boost::future<std::shared_ptr<erizo::SdpInfo>> fut) {
        std::shared_ptr<erizo::SdpInfo> sdp_info = fut.get();
        if (sdp_info) {
          std::shared_ptr<erizo::SdpInfo> sdp_info_copy = std::make_shared<erizo::SdpInfo>(*sdp_info.get());
          ResultVariant result = sdp_info_copy;
          obj->notifyFuture(persistent, result);
        } else {
          obj->notifyFuture(persistent);
        }
        });
  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_METHOD(WebRtcConnection::copySdpToLocalDescription) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }

  ConnectionDescription* source =
    Nan::ObjectWrap::Unwrap<ConnectionDescription>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

  std::shared_ptr<erizo::SdpInfo> source_sdp = source->me;

  me->copyDataToLocalSdpInfo(source_sdp);
}

NAN_METHOD(WebRtcConnection::addRemoteCandidate) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }
  CandidateInfo cand;

  Nan::Utf8String param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string mid = std::string(*param);

  int sdpMLine = Nan::To<int>(info[1]).FromJust();

  Nan::Utf8String param1(Nan::To<v8::String>(info[2]).ToLocalChecked());
  cand.foundation = std::string(*param1);

  cand.componentId = Nan::To<int>(info[3]).FromJust();
  cand.priority = Nan::To<int>(info[4]).FromJust();

  Nan::Utf8String param2(Nan::To<v8::String>(info[5]).ToLocalChecked());
  cand.netProtocol = std::string(*param2);

  Nan::Utf8String param3(Nan::To<v8::String>(info[6]).ToLocalChecked());
  cand.hostAddress = std::string(*param3);

  cand.hostPort = Nan::To<int>(info[7]).FromJust();

  Nan::Utf8String param4(Nan::To<v8::String>(info[8]).ToLocalChecked());
  std::string type = std::string(*param4);

  if (type == "host") {
    cand.hostType = HostType::HOST;
  } else if (type == "srflx") {
    cand.hostType = HostType::SRFLX;
  } else if (type == "prflx") {
    cand.hostType = HostType::PRFLX;
  } else if (type == "relay") {
    cand.hostType = HostType::RELAY;
  } else {
    cand.hostType = HostType::HOST;
  }

  Nan::Utf8String param5(Nan::To<v8::String>(info[9]).ToLocalChecked());
  cand.rAddress = std::string(*param5);

  cand.rPort = Nan::To<int>(info[10]).FromJust();

  Nan::Utf8String param6(Nan::To<v8::String>(info[11]).ToLocalChecked());
  cand.sdp = std::string(*param6);

  me->addRemoteCandidate(mid, sdpMLine, cand);

  info.GetReturnValue().Set(Nan::New(true));
}

NAN_METHOD(WebRtcConnection::getCurrentState) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }

  int state = me->getCurrentState();

  info.GetReturnValue().Set(Nan::New(state));
}

NAN_METHOD(WebRtcConnection::getConnectionQualityLevel) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }

  int level = me->getConnectionQualityLevel();

  info.GetReturnValue().Set(Nan::New(level));
}

NAN_METHOD(WebRtcConnection::addMediaStream) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }

  MediaStream* param = Nan::ObjectWrap::Unwrap<MediaStream>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  auto ms = std::shared_ptr<erizo::MediaStream>(param->me);

  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(Nan::GetCurrentContext()).ToLocalChecked();
  Nan::Persistent<v8::Promise::Resolver> *persistent = new Nan::Persistent<v8::Promise::Resolver>(resolver);
  obj->Ref();
  me->addMediaStream(ms).then(
    [persistent, obj] (boost::future<void>) {
      obj->notifyFuture(persistent);
    });

  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_METHOD(WebRtcConnection::removeMediaStream) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  if (!me) {
    return;
  }

  Nan::Utf8String param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string stream_id = std::string(*param);

  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(Nan::GetCurrentContext()).ToLocalChecked();
  Nan::Persistent<v8::Promise::Resolver> *persistent = new Nan::Persistent<v8::Promise::Resolver>(resolver);
  obj->Ref();
  me->removeMediaStream(stream_id).then(
    [persistent, obj] (boost::future<void>) {
      obj->notifyFuture(persistent);
    });

  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_METHOD(WebRtcConnection::getStats) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  std::shared_ptr<erizo::WebRtcConnection> me = obj->me;
  Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
  if (!me || info.Length() != 1 || obj->closed_) {
    Local<Value> argv[] = {
      Nan::New<v8::String>("{}").ToLocalChecked()
    };
    Nan::Call(*callback, 1, argv);
    return;
  }
  AsyncQueueWorker(new ConnectionStatCallWorker(callback, obj->me));
}

// Async methods

void WebRtcConnection::notifyEvent(erizo::WebRTCEvent event, const std::string& message) {
  boost::mutex::scoped_lock lock(mutex);
  if (!async_) {
    return;
  }
  event_status.push(event);
  event_messages.push(message);
  async_->data = this;
  uv_async_send(async_);
}

NAUV_WORK_CB(WebRtcConnection::eventsCallback) {
  Nan::HandleScope scope;
  WebRtcConnection* obj = reinterpret_cast<WebRtcConnection*>(async->data);
  if (!obj || !obj->me) {
    return;
  }
  boost::mutex::scoped_lock lock(obj->mutex);
  ELOG_DEBUG("%s, message: eventsCallback", obj->toLog());
  while (!obj->event_status.empty()) {
    Local<Value> args[] = {Nan::New(obj->event_status.front()),
                           Nan::New(obj->event_messages.front().c_str()).ToLocalChecked()};
    Nan::AsyncResource resource("erizo::addon.connection.eventsCallback");
    obj->event_callback_->Call(2, args, &resource);
    obj->event_messages.pop();
    obj->event_status.pop();
  }
  ELOG_DEBUG("%s, message: eventsCallback finished", obj->toLog());
}

void WebRtcConnection::notifyFuture(Nan::Persistent<v8::Promise::Resolver> *persistent, ResultVariant result) {
  boost::mutex::scoped_lock lock(mutex);
  if (!future_async_) {
    return;
  }
  ResultPair result_pair(persistent, result);
  futures.push(result_pair);
  future_async_->data = this;
  uv_async_send(future_async_);
}

NAUV_WORK_CB(WebRtcConnection::promiseResolver) {
  Nan::HandleScope scope;
  WebRtcConnection* obj = reinterpret_cast<WebRtcConnection*>(async->data);
  if (!obj || !obj->me) {
    return;
  }
  boost::mutex::scoped_lock lock(obj->mutex);
  ELOG_DEBUG("%s, message: promiseResolver", obj->toLog());
  obj->futures_manager_.cleanResolvedFutures();
  while (!obj->futures.empty()) {
    auto persistent = obj->futures.front().first;
    v8::Local<v8::Promise::Resolver> resolver = Nan::New(*persistent);
    ResultVariant r = obj->futures.front().second;
    if (boost::get<std::string>(&r) != nullptr) {
      resolver->Resolve(Nan::GetCurrentContext(), Nan::New(boost::get<std::string>(r).c_str()).ToLocalChecked())
        .IsNothing();
    } else if (boost::get<std::shared_ptr<erizo::SdpInfo>>(&r) != nullptr) {
      std::shared_ptr<erizo::SdpInfo> sdp_info = boost::get<std::shared_ptr<erizo::SdpInfo>>(r);
      v8::Local<v8::Object> instance = ConnectionDescription::NewInstance();
      ConnectionDescription* description = ObjectWrap::Unwrap<ConnectionDescription>(instance);
      description->me = sdp_info;
      resolver->Resolve(Nan::GetCurrentContext(), instance).IsNothing();
    } else {
      ELOG_WARN("%s, message: Resolving promise with no valid value, using empty string", obj->toLog());
      resolver->Resolve(Nan::GetCurrentContext(), Nan::New("").ToLocalChecked()).IsNothing();
    }
    obj->futures.pop();
    obj->Unref();
  }
  ELOG_DEBUG("%s, message: promiseResolver finished", obj->toLog());
}
