#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "IOWorker.h"

using v8::Local;
using v8::Value;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Exception;

Nan::Persistent<Function> IOWorker::constructor;

IOWorker::IOWorker() {
}

IOWorker::~IOWorker() {
}

NAN_MODULE_INIT(IOWorker::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("IOWorker").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "start", start);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("IOWorker").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(IOWorker::New) {
  IOWorker* obj = new IOWorker();
  obj->me = std::make_shared<erizo::IOWorker>();

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(IOWorker::close) {
  IOWorker* obj = Nan::ObjectWrap::Unwrap<IOWorker>(info.Holder());

  obj->me->close();
}

NAN_METHOD(IOWorker::start) {
  IOWorker* obj = Nan::ObjectWrap::Unwrap<IOWorker>(info.Holder());

  obj->me->start();
}
