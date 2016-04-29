#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "ExternalOutput.h"


using namespace v8;

Nan::Persistent<Function> ExternalOutput::constructor;

ExternalOutput::ExternalOutput() {};
ExternalOutput::~ExternalOutput() {};

void ExternalOutput::Init(v8::Local<v8::Object> exports) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ExternalOutput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", close);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("ExternalOutput").ToLocalChecked(), tpl->GetFunction());
}

void ExternalOutput::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);

  ExternalOutput* obj = new ExternalOutput();
  obj->me = new erizo::ExternalOutput(url);

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

void ExternalOutput::close(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(info.Holder());
  erizo::ExternalOutput *me = (erizo::ExternalOutput*)obj->me;

  delete me;
}

void ExternalOutput::init(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(info.Holder());
  erizo::ExternalOutput *me = (erizo::ExternalOutput*)obj->me;

  int r = me->init();
  info.GetReturnValue().Set(Nan::New(r));
}

