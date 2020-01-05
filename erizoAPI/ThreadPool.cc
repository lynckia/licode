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

using erizo::DurationDistribution;

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
  Nan::SetPrototypeMethod(tpl, "getDurationDistribution", getDurationDistribution);
  Nan::SetPrototypeMethod(tpl, "resetStats", resetStats);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("ThreadPool").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ThreadPool::New) {
  if (info.Length() < 1) {
    Nan::ThrowError("Wrong number of arguments");
  }

  unsigned int num_workers = Nan::To<unsigned int>(info[0]).FromJust();

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

NAN_METHOD(ThreadPool::getDurationDistribution) {
  ThreadPool* obj = Nan::ObjectWrap::Unwrap<ThreadPool>(info.Holder());
  DurationDistribution duration_distribution = obj->me->getDurationDistribution();
  v8::Local<v8::Array> array = Nan::New<v8::Array>(5);
  Nan::Set(array, 0, Nan::New(duration_distribution.duration_0_10_ms));
  Nan::Set(array, 1, Nan::New(duration_distribution.duration_10_50_ms));
  Nan::Set(array, 2, Nan::New(duration_distribution.duration_50_100_ms));
  Nan::Set(array, 3, Nan::New(duration_distribution.duration_100_1000_ms));
  Nan::Set(array, 4, Nan::New(duration_distribution.duration_1000_ms));

  info.GetReturnValue().Set(array);
}

NAN_METHOD(ThreadPool::resetStats) {
  ThreadPool* obj = Nan::ObjectWrap::Unwrap<ThreadPool>(info.Holder());
  obj->me->resetStats();
}
