#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "ExternalOutput.h"


using namespace v8;

Nan::Persistent<Function> ExternalOutput::constructor;

ExternalOutput::ExternalOutput() {};
ExternalOutput::~ExternalOutput() {};

NAN_MODULE_INIT (ExternalOutput::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ExternalOutput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", close);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("ExternalOutput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ExternalOutput::New) {
  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);

  ExternalOutput* obj = new ExternalOutput();
  obj->me = new erizo::ExternalOutput(url);

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ExternalOutput::close) {
  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(info.Holder());
  erizo::ExternalOutput *me = (erizo::ExternalOutput*)obj->me;

  delete me;
}

NAN_METHOD(ExternalOutput::init) {
  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(info.Holder());
  erizo::ExternalOutput *me = (erizo::ExternalOutput*)obj->me;

  int r = me->init();
  info.GetReturnValue().Set(Nan::New(r));
}

