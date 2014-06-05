#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "RtpSink.h"


using namespace v8;

RtpSink::RtpSink() {};
RtpSink::~RtpSink() {};

void RtpSink::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("RtpSink"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("close"), FunctionTemplate::New(close)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("init"), FunctionTemplate::New(init)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("RtpSink"), constructor);
}

Handle<Value> RtpSink::New(const Arguments& args) {
  HandleScope scope;

  v8::String::Utf8Value param(args[0]->ToString());
  std::string url = std::string(*param);
  
  v8::String::Utf8Value param1(args[1]->ToString());
  std::string port = std::string(*param1);

  int fbPort = args[2]->IntegerValue();
  
  RtpSink* obj = new RtpSink();
  obj->me = new erizo::RtpSink(url, port, fbPort);

  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> RtpSink::close(const Arguments& args) {
  HandleScope scope;

  RtpSink* obj = ObjectWrap::Unwrap<RtpSink>(args.This());
  erizo::RtpSink *me = (erizo::RtpSink*)obj->me;

  delete me;

  return scope.Close(Null());
}

Handle<Value> RtpSink::init(const Arguments& args) {
  HandleScope scope;


  int r = 0;
  return scope.Close(Integer::New(r));
}

