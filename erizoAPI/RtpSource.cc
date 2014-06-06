#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "RtpSource.h"


using namespace v8;

RtpSource::RtpSource() {};
RtpSource::~RtpSource() {};

void RtpSource::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("RtpSource"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("close"), FunctionTemplate::New(close)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("init"), FunctionTemplate::New(init)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setAudioReceiver"), FunctionTemplate::New(setAudioReceiver)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setVideoReceiver"), FunctionTemplate::New(setVideoReceiver)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("RtpSource"), constructor);
}

Handle<Value> RtpSource::New(const Arguments& args) {
  HandleScope scope;

  int mediaPort = args[0]->IntegerValue(); 

  v8::String::Utf8Value param(args[1]->ToString());
  std::string fbUrl = std::string(*param);
  v8::String::Utf8Value param1(args[2]->ToString());
  std::string fbPort = std::string(*param1);

  RtpSource* obj = new RtpSource();
  obj->me = new erizo::RtpSource(mediaPort, fbUrl, fbPort);

  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> RtpSource::close(const Arguments& args) {
  HandleScope scope;

  RtpSource* obj = ObjectWrap::Unwrap<RtpSource>(args.This());
  erizo::RtpSource *me = (erizo::RtpSource*)obj->me;

  delete me;

  return scope.Close(Null());
}

Handle<Value> RtpSource::init(const Arguments& args) {
  HandleScope scope;

  RtpSource* obj = ObjectWrap::Unwrap<RtpSource>(args.This());
  erizo::RtpSource *me = (erizo::RtpSource*) obj->me;

//  int r = me->init();

  return scope.Close(Integer::New(0));
}

Handle<Value> RtpSource::setAudioReceiver(const Arguments& args) {
  HandleScope scope;

  RtpSource* obj = ObjectWrap::Unwrap<RtpSource>(args.This());
  erizo::RtpSource *me = (erizo::RtpSource*)obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(args[0]->ToObject());
  erizo::MediaSink *mr = param->msink;

  me->setAudioSink(mr);

  return scope.Close(Null());
}

Handle<Value> RtpSource::setVideoReceiver(const Arguments& args) {
  HandleScope scope;

  RtpSource* obj = ObjectWrap::Unwrap<RtpSource>(args.This());
  erizo::RtpSource *me = (erizo::RtpSource*)obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(args[0]->ToObject());
  erizo::MediaSink *mr = param->msink;

  me->setVideoSink(mr);

  return scope.Close(Null());
}
