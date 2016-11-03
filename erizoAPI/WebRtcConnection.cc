#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "WebRtcConnection.h"

using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Persistent;
using v8::Exception;
using v8::Value;

Nan::Persistent<Function> WebRtcConnection::constructor;

WebRtcConnection::WebRtcConnection() {
}

WebRtcConnection::~WebRtcConnection() {
  if (me != NULL) {
    me->setWebRtcConnectionStatsListener(NULL);
    me->setWebRtcConnectionEventListener(NULL);
  }
}

NAN_MODULE_INIT(WebRtcConnection::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("WebRtcConnection").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);
  Nan::SetPrototypeMethod(tpl, "setRemoteSdp", setRemoteSdp);
  Nan::SetPrototypeMethod(tpl, "addRemoteCandidate", addRemoteCandidate);
  Nan::SetPrototypeMethod(tpl, "getLocalSdp", getLocalSdp);
  Nan::SetPrototypeMethod(tpl, "setAudioReceiver", setAudioReceiver);
  Nan::SetPrototypeMethod(tpl, "setVideoReceiver", setVideoReceiver);
  Nan::SetPrototypeMethod(tpl, "getCurrentState", getCurrentState);
  Nan::SetPrototypeMethod(tpl, "getStats", getStats);
  Nan::SetPrototypeMethod(tpl, "generatePLIPacket", generatePLIPacket);
  Nan::SetPrototypeMethod(tpl, "setFeedbackReports", setFeedbackReports);
  Nan::SetPrototypeMethod(tpl, "createOffer", createOffer);
  Nan::SetPrototypeMethod(tpl, "setSlideShowMode", setSlideShowMode);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("WebRtcConnection").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}


NAN_METHOD(WebRtcConnection::New) {
  if (info.Length() < 8) {
    ThrowException(Exception::TypeError(v8::String::New("Wrong number of arguments")));
  }
  // webrtcconnection(std::string id, bool audioEnabled, bool videoEnabled,
  // const std::string &stunServer, int stunPort, int minPort, int maxPort);

  if (info.IsConstructCall()) {
    // Invoked as a constructor with 'new WebRTC()'

    v8::String::Utf8Value paramId(Nan::To<v8::String>(info[0]).ToLocalChecked());
    std::string wrtcId = std::string(*paramId);
    bool a = info[1]->BooleanValue();
    bool v = info[2]->BooleanValue();
    v8::String::Utf8Value param(Nan::To<v8::String>(info[3]).ToLocalChecked());
    std::string stunServer = std::string(*param);
    int stunPort = info[4]->IntegerValue();
    int minPort = info[5]->IntegerValue();
    int maxPort = info[6]->IntegerValue();
    bool trickle = (info[7]->ToBoolean())->BooleanValue();

    erizo::IceConfig iceConfig;
    if (info.Length() == 12) {
      v8::String::Utf8Value param2(Nan::To<v8::String>(info[8]).ToLocalChecked());
      std::string turnServer = std::string(*param2);
      int turnPort = info[9]->IntegerValue();
      v8::String::Utf8Value param3(Nan::To<v8::String>(info[10]).ToLocalChecked());
      std::string turnUsername = std::string(*param3);
      v8::String::Utf8Value param4(Nan::To<v8::String>(info[11]).ToLocalChecked());
      std::string turnPass = std::string(*param4);
      iceConfig.turnServer = turnServer;
      iceConfig.turnPort = turnPort;
      iceConfig.turnUsername = turnUsername;
      iceConfig.turnPass = turnPass;
    }


    iceConfig.stunServer = stunServer;
    iceConfig.stunPort = stunPort;
    iceConfig.minPort = minPort;
    iceConfig.maxPort = maxPort;
    iceConfig.shouldTrickle = trickle;

    WebRtcConnection* obj = new WebRtcConnection();
    obj->me = new erizo::WebRtcConnection(wrtcId, a, v, iceConfig, obj);
    obj->msink = obj->me;
    uv_async_init(uv_default_loop(), &obj->async_, &WebRtcConnection::eventsCallback);
    uv_async_init(uv_default_loop(), &obj->asyncStats_, &WebRtcConnection::statsCallback);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // TODO(pedro) Check what happens here
  }
}

NAN_METHOD(WebRtcConnection::close) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  obj->me->setWebRtcConnectionStatsListener(NULL);
  obj->me->setWebRtcConnectionEventListener(NULL);
  obj->me = NULL;
  obj->hasCallback_ = false;

  if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&obj->async_))) {
    uv_close(reinterpret_cast<uv_handle_t*>(&obj->async_), NULL);
  }
  if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&obj->asyncStats_))) {
    uv_close(reinterpret_cast<uv_handle_t*>(&obj->asyncStats_), NULL);
  }
}

NAN_METHOD(WebRtcConnection::init) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  obj->eventCallback_ = Persistent<Function>::New(Local<Function>::Cast(info[0]));
  bool r = me->init();

  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(WebRtcConnection::createOffer) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;
  bool r = me->createOffer();
  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(WebRtcConnection::setSlideShowMode) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  bool v = info[0]->BooleanValue();
  me->setSlideShowMode(v);
  info.GetReturnValue().Set(Nan::New(v));
}

NAN_METHOD(WebRtcConnection::setRemoteSdp) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string sdp = std::string(*param);

  bool r = me->setRemoteSdp(sdp);

  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(WebRtcConnection::addRemoteCandidate) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string mid = std::string(*param);

  int sdpMLine = info[1]->IntegerValue();

  v8::String::Utf8Value param2(Nan::To<v8::String>(info[2]).ToLocalChecked());
  std::string sdp = std::string(*param2);

  bool r = me->addRemoteCandidate(mid, sdpMLine, sdp);

  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(WebRtcConnection::getLocalSdp) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  std::string sdp = me->getLocalSdp();

  info.GetReturnValue().Set(Nan::New(sdp.c_str()).ToLocalChecked());
}


NAN_METHOD(WebRtcConnection::setAudioReceiver) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  MediaSink* param = Nan::ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setAudioSink(mr);
}

NAN_METHOD(WebRtcConnection::setVideoReceiver) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  MediaSink* param = Nan::ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setVideoSink(mr);
}

NAN_METHOD(WebRtcConnection::getCurrentState) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  int state = me->getCurrentState();

  info.GetReturnValue().Set(Nan::New(state));
}

NAN_METHOD(WebRtcConnection::getStats) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  if (obj->me == NULL) {  // Requesting stats when WebrtcConnection not available
    return;
  }
  if (info.Length() == 0) {
    std::string lastStats = obj->me->getJSONStats();
    info.GetReturnValue().Set(Nan::New(lastStats.c_str()).ToLocalChecked());
  } else {
    obj->me->setWebRtcConnectionStatsListener(obj);
    obj->hasCallback_ = true;
    obj->statsCallback_ = Persistent<Function>::New(Local<Function>::Cast(info[0]));
  }
}

NAN_METHOD(WebRtcConnection::generatePLIPacket) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());

  if (obj->me == NULL) {
    return;
  }

  erizo::WebRtcConnection *me = obj->me;
  me->sendPLI();
  return;
}

NAN_METHOD(WebRtcConnection::setFeedbackReports) {
  WebRtcConnection* obj = Nan::ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  bool v = info[0]->BooleanValue();
  int fbreps = info[1]->IntegerValue();  // From bps to Kbps
  me->setFeedbackReports(v, fbreps);
}

// Async methods

void WebRtcConnection::notifyEvent(erizo::WebRTCEvent event, const std::string& message) {
  boost::mutex::scoped_lock lock(mutex);
  this->eventSts.push(event);
  this->eventMsgs.push(message);
  async_.data = this;
  uv_async_send(&async_);
}

void WebRtcConnection::notifyStats(const std::string& message) {
  if (!this->hasCallback_) {
    return;
  }
  boost::mutex::scoped_lock lock(mutex);
  this->statsMsgs.push(message);
  asyncStats_.data = this;
  uv_async_send(&asyncStats_);
}

void WebRtcConnection::eventsCallback(uv_async_t *handle, int status) {
  HandleScope scope;
  WebRtcConnection* obj = reinterpret_cast<WebRtcConnection*>(handle->data);
  if (!obj || obj->me == NULL)
    return;
  boost::mutex::scoped_lock lock(obj->mutex);
  while (!obj->eventSts.empty()) {
    Local<Value> args[] = {Nan::New(obj->eventSts.front()), Nan::New(obj->eventMsgs.front().c_str()).ToLocalChecked()};
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), Nan::New(obj->eventCallback_), 2, args);
    obj->eventMsgs.pop();
    obj->eventSts.pop();
  }
}

void WebRtcConnection::statsCallback(uv_async_t *handle, int status) {
  HandleScope scope;
  WebRtcConnection* obj = reinterpret_cast<WebRtcConnection*>(handle->data);
  if (!obj || obj->me == NULL)
    return;
  boost::mutex::scoped_lock lock(obj->mutex);
  if (obj->hasCallback_) {
    while (!obj->statsMsgs.empty()) {
      Local<Value> args[] = {Nan::New(obj->statsMsgs.front().c_str()).ToLocalChecked()};
      Nan::MakeCallback(Nan::GetCurrentContext()->Global(), Nan::New(obj->statsCallback_), 1, args);
      obj->statsMsgs.pop();
    }
  }
}
