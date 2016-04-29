#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "ExternalInput.h"


using namespace v8;

ExternalInput::ExternalInput() {};
ExternalInput::~ExternalInput() {};

void ExternalInput::Init(v8::Local<v8::Object> exports) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ExternalInput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", close);
  Nan::SetPrototypeMethod(tpl, "setAudioReceiver", close);
  Nan::SetPrototypeMethod(tpl, "setVideoReceiver", close);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("ExternalInput").ToLocalChecked(), tpl->GetFunction());
}

void ExternalInput::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);

  ExternalInput* obj = new ExternalInput();
  obj->me = new erizo::ExternalInput(url);
  
  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

void ExternalInput::close(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  delete me;
}

void ExternalInput::init(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  int r = me->init();
  info.GetReturnValue().Set(Nan::New(r));
}

void ExternalInput::setAudioReceiver(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setAudioSink(mr);
}

void ExternalInput::setVideoReceiver(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setVideoSink(mr);
}
