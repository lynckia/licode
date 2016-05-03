#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "ExternalInput.h"


using namespace v8;

Nan::Persistent<Function> ExternalInput::constructor;

ExternalInput::ExternalInput() {};
ExternalInput::~ExternalInput() {};

NAN_MODULE_INIT (ExternalInput::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ExternalInput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);
  Nan::SetPrototypeMethod(tpl, "setAudioReceiver", setAudioReceiver);
  Nan::SetPrototypeMethod(tpl, "setVideoReceiver", setVideoReceiver);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("ExternalInput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ExternalInput::New) {
  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);

  ExternalInput* obj = new ExternalInput();
  obj->me = new erizo::ExternalInput(url);
  
  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ExternalInput::close) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  delete me;
}

NAN_METHOD(ExternalInput::init) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  int r = me->init();
  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(ExternalInput::setAudioReceiver) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setAudioSink(mr);
}

NAN_METHOD(ExternalInput::setVideoReceiver) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setVideoSink(mr);
}
