#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "ThreadPool.h"

using v8::Local;
using v8::Value;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Exception;

Nan::Persistent<Function> ThreadPool::constructor;

ThreadPool::ThreadPool() {
}

ThreadPool::~ThreadPool() {
}

NAN_MODULE_INIT(ThreadPool::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ThreadPool").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "start", start);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("ThreadPool").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ThreadPool::New) {
  if (info.Length() < 1) {
    Nan::ThrowError("Wrong number of arguments");
  }

  unsigned int num_workers = info[0]->IntegerValue();

  ThreadPool* obj = new ThreadPool();
  obj->me.reset(new erizo::ThreadPool(num_workers));

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ThreadPool::close) {
  ThreadPool* obj = Nan::ObjectWrap::Unwrap<ThreadPool>(info.Holder());

  obj->me->close();
}

NAN_METHOD(ThreadPool::start) {
  ThreadPool* obj = Nan::ObjectWrap::Unwrap<ThreadPool>(info.Holder());

  obj->me->start();
}
