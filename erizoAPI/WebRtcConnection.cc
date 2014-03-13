#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "WebRtcConnection.h"


using namespace v8;

WebRtcConnection::WebRtcConnection() {};
WebRtcConnection::~WebRtcConnection() {};
//bool WebRtcConnection::initialized = false;

void WebRtcConnection::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("WebRtcConnection"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("close"), FunctionTemplate::New(close)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("init"), FunctionTemplate::New(init)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setRemoteSdp"), FunctionTemplate::New(setRemoteSdp)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("getLocalSdp"), FunctionTemplate::New(getLocalSdp)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setAudioReceiver"), FunctionTemplate::New(setAudioReceiver)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setVideoReceiver"), FunctionTemplate::New(setVideoReceiver)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("getCurrentState"), FunctionTemplate::New(getCurrentState)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("WebRtcConnection"), constructor);
/*
  if (!initialized){
    context_obj = Persistent<Object>::New(Object::New());
    target->Set(String::New("webrtcEvent"),context_obj);
    initialized = true;
  }
  */
}


Handle<Value> WebRtcConnection::New(const Arguments& args) {
  HandleScope scope;
  if (args.Length()<4){
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return args.This();
  }
//	webrtcconnection(bool audioEnabled, bool videoEnabled, const std::string &stunServer, int stunPort, int minPort, int maxPort);

  bool a = (args[0]->ToBoolean())->BooleanValue();
  bool v = (args[1]->ToBoolean())->BooleanValue();
  String::Utf8Value param(args[2]->ToString());
  std::string stunServer = std::string(*param);
  int stunPort = args[3]->IntegerValue();
  int minPort = args[4]->IntegerValue();
  int maxPort = args[5]->IntegerValue();
  WebRtcConnection* obj = new WebRtcConnection();
  obj->me = new erizo::WebRtcConnection(a, v, stunServer,stunPort,minPort,maxPort);
  obj->me->setWebRtcConnectionEventListener(obj);
  obj->me->setWebRtcConnectionStatsListener(obj);
  obj->Wrap(args.This());
  uv_async_init(uv_default_loop(), &obj->async, &WebRtcConnection::after_cb); 
  obj->message = 0;
  return args.This();
}

Handle<Value> WebRtcConnection::close(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;
  uv_close((uv_handle_t*)&obj->async, NULL);

  delete me;

  return scope.Close(Null());
}

Handle<Value> WebRtcConnection::init(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  bool r = me->init();

  obj->hasCallback_ = true;
  obj->eventCallback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
  
  return scope.Close(Boolean::New(r));
}

Handle<Value> WebRtcConnection::setRemoteSdp(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  String::Utf8Value param(args[0]->ToString());
  std::string sdp = std::string(*param);

  bool r = me->setRemoteSdp(sdp);

  return scope.Close(Boolean::New(r));
}

Handle<Value> WebRtcConnection::getLocalSdp(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  std::string sdp = me->getLocalSdp();

  return scope.Close(String::NewSymbol(sdp.c_str()));
}


Handle<Value> WebRtcConnection::setAudioReceiver(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(args[0]->ToObject());
  erizo::MediaSink *mr = param->msink;

  me-> setAudioSink(mr);

  return scope.Close(Null());
}

Handle<Value> WebRtcConnection::setVideoReceiver(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(args[0]->ToObject());
  erizo::MediaSink *mr = param->msink;

  me-> setVideoSink(mr);

  return scope.Close(Null());
}

Handle<Value> WebRtcConnection::getCurrentState(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  int state = me->getCurrentState();

  return scope.Close(Number::New(state));
}

void WebRtcConnection::notifyEvent(erizo::WebRTCEvent event, const std::string& message) {
  this->message=event;
  async.data = this;
  uv_async_send (&async);
}

void WebRtcConnection::notifyStats(const std::string& message) {
  /*
  this->message=event;
  async.data = this;
  uv_async_send (&async);
  */
}

void WebRtcConnection::after_cb(uv_async_t *handle, int status){

  HandleScope scope;
  WebRtcConnection* obj = (WebRtcConnection*)handle->data;
  printf("WebRTC Update received %d\n", obj->message);
  Local<Value> args[] = {Integer::New(obj->message)};
  if (obj->hasCallback_) 
    obj->eventCallback->Call(Context::GetCurrent()->Global(), 1, args);
}
