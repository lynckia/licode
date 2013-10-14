#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include <node.h>
#include "WebRtcConnection.h"


using namespace v8;

WebRtcConnection::WebRtcConnection() {};
WebRtcConnection::~WebRtcConnection() {};

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
}

Handle<Value> WebRtcConnection::New(const Arguments& args) {
  HandleScope scope;

  bool a = (args[0]->ToBoolean())->BooleanValue();
  bool v = (args[1]->ToBoolean())->BooleanValue();

  WebRtcConnection* obj = new WebRtcConnection();
  obj->me = new erizo::WebRtcConnection(a, v);
  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> WebRtcConnection::close(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  delete me;

  return scope.Close(Null());
}

Handle<Value> WebRtcConnection::init(const Arguments& args) {
  HandleScope scope;

  WebRtcConnection* obj = ObjectWrap::Unwrap<WebRtcConnection>(args.This());
  erizo::WebRtcConnection *me = obj->me;

  bool r = me->init();

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
