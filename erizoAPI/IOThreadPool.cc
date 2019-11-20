#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "IOThreadPool.h"

using v8::Local;
using v8::Value;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Exception;

Nan::Persistent<Function> IOThreadPool::constructor;

IOThreadPool::IOThreadPool() {
}

IOThreadPool::~IOThreadPool() {
}

NAN_MODULE_INIT(IOThreadPool::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("IOThreadPool").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "start", start);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("IOThreadPool").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(IOThreadPool::New) {
  if (info.Length() < 1) {
    Nan::ThrowError("Wrong number of arguments");
  }

  unsigned int num_workers = Nan::To<unsigned int>(info[0]).FromJust();

  IOThreadPool* obj = new IOThreadPool();
  obj->me.reset(new erizo::IOThreadPool(num_workers));

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(IOThreadPool::close) {
  IOThreadPool* obj = Nan::ObjectWrap::Unwrap<IOThreadPool>(info.Holder());

  obj->me->close();
}

NAN_METHOD(IOThreadPool::start) {
  IOThreadPool* obj = Nan::ObjectWrap::Unwrap<IOThreadPool>(info.Holder());

  obj->me->start();
}
