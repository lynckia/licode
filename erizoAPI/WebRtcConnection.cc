#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "WebRtcConnection.h"

using namespace v8;

Nan::Persistent<Function> WebRtcConnection::constructor;

WebRtcConnection::WebRtcConnection() {
};

WebRtcConnection::~WebRtcConnection() {
};

void WebRtcConnection::Init(Local<Object> exports) {
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
  exports->Set(Nan::New("WebRtcConnection").ToLocalChecked(), tpl->GetFunction());
}


void WebRtcConnection::New(const Nan::FunctionCallbackInfo<Value>& info) {
 
  /*  TODO: Control number of args
  if (args.Length()<7){
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
  }
  */
  //	webrtcconnection(bool audioEnabled, bool videoEnabled, const std::string &stunServer, int stunPort, int minPort, int maxPort);
  
  if (info.IsConstructCall()){
    //Invoked as a constructor with 'new WebRTC()'
    bool a = info[0]->BooleanValue();
    bool v = info[1]->BooleanValue();
    String::Utf8Value param(Nan::To<v8::String>(info[2]).ToLocalChecked());
    std::string stunServer = std::string(*param);
    int stunPort = info[3]->IntegerValue();
    int minPort = info[4]->IntegerValue();
    int maxPort = info[5]->IntegerValue();
    bool t = (info[6]->ToBoolean())->BooleanValue();

    erizo::IceConfig iceConfig;
    if (info.Length()==11){
      String::Utf8Value param2(Nan::To<v8::String>(info[7]).ToLocalChecked());
      std::string turnServer = std::string(*param2);
      int turnPort = info[8]->IntegerValue();
      String::Utf8Value param3(Nan::To<v8::String>(info[9]).ToLocalChecked());
      std::string turnUsername = std::string(*param3);
      String::Utf8Value param4(Nan::To<v8::String>(info[10]).ToLocalChecked());
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

    WebRtcConnection* obj = new WebRtcConnection();
    obj->me = new erizo::WebRtcConnection(a, v, iceConfig,t, obj);
    obj->msink = obj->me;
    uv_async_init(uv_default_loop(), &obj->async_, &WebRtcConnection::eventsCallback); 
    uv_async_init(uv_default_loop(), &obj->asyncStats_, &WebRtcConnection::statsCallback); 
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
   //TODO: Check what happens here  
  }
}

void WebRtcConnection::close(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  obj->me = NULL;
  obj->hasCallback_ = false;

  if(!uv_is_closing((uv_handle_t*)&obj->async_)) {
    uv_close((uv_handle_t*)&obj->async_, NULL);
  }
  if(!uv_is_closing((uv_handle_t*)&obj->asyncStats_)) {
    uv_close((uv_handle_t*)&obj->asyncStats_, NULL);
  }
}

void WebRtcConnection::init(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;
  
  obj->eventCallback_ = Persistent<Function>::New(Local<Function>::Cast(info[0]));
  bool r = me->init();
  
  info.GetReturnValue().Set(Nan::New(r));
}

void WebRtcConnection::createOffer(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;
  bool r = me->createOffer();
  info.GetReturnValue().Set(Nan::New(r));
}

void WebRtcConnection::setSlideShowMode(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;
  
  bool v = info[0]->BooleanValue();
  me->setSlideShowMode(v);
  info.GetReturnValue().Set(Nan::New(v));
}

void WebRtcConnection::setRemoteSdp(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string sdp = std::string(*param);

  bool r = me->setRemoteSdp(sdp);

  info.GetReturnValue().Set(Nan::New(r));
}

void  WebRtcConnection::addRemoteCandidate(const Nan::FunctionCallbackInfo<v8::Value>& info) {

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string mid = std::string(*param);

  int sdpMLine = info[1]->IntegerValue();
  
  String::Utf8Value param2(Nan::To<v8::String>(info[2]).ToLocalChecked());
  std::string sdp = std::string(*param2);

  bool r = me->addRemoteCandidate(mid, sdpMLine, sdp);

  info.GetReturnValue().Set(Nan::New(r));
}

void WebRtcConnection::getLocalSdp(const Nan::FunctionCallbackInfo<v8::Value>& info) {

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  std::string sdp = me->getLocalSdp();
  
  info.GetReturnValue().Set(Nan::New(sdp.c_str()).ToLocalChecked());
}


void WebRtcConnection::setAudioReceiver(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me-> setAudioSink(mr);
}

void WebRtcConnection::setVideoReceiver(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me-> setVideoSink(mr);
}

void WebRtcConnection::getCurrentState(const Nan::FunctionCallbackInfo<v8::Value>& info) {

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;

  int state = me->getCurrentState();
  
  info.GetReturnValue().Set(Nan::New(state));
}

void WebRtcConnection::getStats(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  if (obj->me == NULL){ //Requesting stats when WebrtcConnection not available
    return; 
  }
  if (info.Length()==0){
    std::string lastStats = obj->me->getJSONStats();
    info.GetReturnValue().Set(Nan::New(lastStats.c_str()).ToLocalChecked());
  }else{
    obj->me->setWebRtcConnectionStatsListener(obj);
    obj->hasCallback_ = true;
    obj->statsCallback_ = Persistent<Function>::New(Local<Function>::Cast(info[0]));
  }
}

void WebRtcConnection::generatePLIPacket(const Nan::FunctionCallbackInfo<v8::Value>& info){
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());

  if(obj->me ==NULL){
    return;
  }

  erizo::WebRtcConnection *me = obj->me;
  me->sendPLI();

  return; 
}

void WebRtcConnection::setFeedbackReports(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(info.Holder());
  erizo::WebRtcConnection *me = obj->me;
  
  bool v = info[0]->BooleanValue();
  int fbreps = info[1]->IntegerValue(); // From bps to Kbps
  me->setFeedbackReports(v, fbreps);
}

void WebRtcConnection::notifyEvent(erizo::WebRTCEvent event, const std::string& message) {
  boost::mutex::scoped_lock lock(mutex);
  this->eventSts.push(event);
  this->eventMsgs.push(message);
  async_.data = this;
  uv_async_send (&async_);
}

void WebRtcConnection::notifyStats(const std::string& message) {
  if (!this->hasCallback_){
    return;
  }
  boost::mutex::scoped_lock lock(mutex);
  this->statsMsgs.push(message);
  asyncStats_.data = this;
  uv_async_send (&asyncStats_);
}

void WebRtcConnection::eventsCallback(uv_async_t *handle, int status) {
  HandleScope scope;
  WebRtcConnection* obj = (WebRtcConnection*)handle->data;
  if (!obj || obj->me == NULL)
    return;
  boost::mutex::scoped_lock lock(obj->mutex);
  while (!obj->eventSts.empty()) {
    Local<Value> args[] = {Nan::New(obj->eventSts.front()),Nan::New(obj->eventMsgs.front().c_str()).ToLocalChecked()};
    Nan::MakeCallback(Nan::GetCurrentContext()->Global(), Nan::New(obj->eventCallback_), 2, args);
    obj->eventMsgs.pop();
    obj->eventSts.pop();
  }
}

void WebRtcConnection::statsCallback(uv_async_t *handle, int status) {

  HandleScope scope;
  WebRtcConnection* obj = (WebRtcConnection*)handle->data;
  if (!obj || obj->me == NULL)
    return;
  boost::mutex::scoped_lock lock(obj->mutex);
  if (obj->hasCallback_) {
    while(!obj->statsMsgs.empty()){
      Local<Value> args[] = {Nan::New(obj->statsMsgs.front().c_str()).ToLocalChecked()};
      Nan::MakeCallback(Nan::GetCurrentContext()->Global(), Nan::New(obj->statsCallback_), 1, args);
      obj->statsMsgs.pop();
    }
  }
}
