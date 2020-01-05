#include <nan.h>
#include "AsyncPromiseWorker.h"

AsyncPromiseWorker::AsyncPromiseWorker(Nan::Persistent<v8::Promise::Resolver> *persistent)
    : AsyncWorker(nullptr) {
  _persistent = persistent;
}

AsyncPromiseWorker::~AsyncPromiseWorker() {}

void AsyncPromiseWorker::Execute() {
}

void AsyncPromiseWorker::HandleOKCallback() {
  Nan::HandleScope scope;
  auto resolver = Nan::New(*_persistent);
  resolver->Resolve(Nan::GetCurrentContext(), Nan::New("").ToLocalChecked()).IsNothing();
}

void AsyncPromiseWorker::HandleErrorCallback() {
  Nan::HandleScope scope;
  auto resolver = Nan::New(*_persistent);
  resolver->Reject(Nan::GetCurrentContext(), Nan::New("").ToLocalChecked()).IsNothing();
}

