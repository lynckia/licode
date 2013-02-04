#define BUILDING_NODE_EXTENSION
#include <node.h>
#include "ExternalInput.h"


using namespace v8;

ExternalInput::ExternalInput() {};
ExternalInput::~ExternalInput() {};

void ExternalInput::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("ExternalInput"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("close"), FunctionTemplate::New(close)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("init"), FunctionTemplate::New(init)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setAudioReceiver"), FunctionTemplate::New(setAudioReceiver)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setVideoReceiver"), FunctionTemplate::New(setVideoReceiver)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("ExternalInput"), constructor);
}

Handle<Value> ExternalInput::New(const Arguments& args) {
  HandleScope scope;

  v8::String::Utf8Value param(args[0]->ToString());
  std::string url = std::string(*param);

  ExternalInput* obj = new ExternalInput();
  obj->me = new erizo::ExternalInput(url);

  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> ExternalInput::close(const Arguments& args) {
  HandleScope scope;

  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(args.This());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  delete me;

  return scope.Close(Null());
}

Handle<Value> ExternalInput::init(const Arguments& args) {
  HandleScope scope;

  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(args.This());
  erizo::ExternalInput *me = (erizo::ExternalInput*) obj->me;

  bool r = me->init();

  return scope.Close(Boolean::New(r));
}

Handle<Value> ExternalInput::setAudioReceiver(const Arguments& args) {
  HandleScope scope;

  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(args.This());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  MediaReceiver* param = ObjectWrap::Unwrap<MediaReceiver>(args[0]->ToObject());
  erizo::MediaReceiver *mr = param->me;

  me-> setAudioReceiver(mr);

  return scope.Close(Null());
}

Handle<Value> ExternalInput::setVideoReceiver(const Arguments& args) {
  HandleScope scope;

  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(args.This());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  MediaReceiver* param = ObjectWrap::Unwrap<MediaReceiver>(args[0]->ToObject());
  erizo::MediaReceiver *mr = param->me;

  me-> setVideoReceiver(mr);

  return scope.Close(Null());
}
