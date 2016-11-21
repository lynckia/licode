#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "ExternalOutput.h"


using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Value;

Nan::Persistent<Function> ExternalOutput::constructor;

class AsyncDeleter : public Nan::AsyncWorker {
 public:
    AsyncDeleter(std::shared_ptr<erizo::ExternalOutput> eoToDelete, Nan::Callback *callback):
      AsyncWorker(callback), eoToDelete_(eoToDelete) {
      }
    ~AsyncDeleter() {}
    void Execute() {
      eoToDelete_.reset();
    }
    void HandleOKCallback() {
      HandleScope scope;
      std::string msg("OK");
      if (callback) {
        Local<Value> argv[] = {
          Nan::New(msg.c_str()).ToLocalChecked()
        };
        callback->Call(1, argv);
      }
    }
 private:
    std::shared_ptr<erizo::ExternalOutput> eoToDelete_;
    Nan::Callback* callback_;
};

ExternalOutput::ExternalOutput() {}
ExternalOutput::~ExternalOutput() {}

NAN_MODULE_INIT(ExternalOutput::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ExternalOutput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("ExternalOutput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ExternalOutput::New) {
  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);

  ExternalOutput* obj = new ExternalOutput();
  obj->me = std::make_shared<erizo::ExternalOutput>(url);

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ExternalOutput::close) {
  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(info.Holder());
  std::shared_ptr<erizo::ExternalOutput> me = obj->me;

  Nan::Callback *callback;
  if (info.Length() >= 1) {
    callback = new Nan::Callback(info[0].As<Function>());
  } else {
    callback = NULL;
  }

  Nan::AsyncQueueWorker(new  AsyncDeleter(me, callback));
  me.reset();
}

NAN_METHOD(ExternalOutput::init) {
  // TODO(pedro) Could potentially be slow, think about async'ing it
  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(info.Holder());
  std::shared_ptr<erizo::ExternalOutput> me = obj->me;

  int r = me->init();
  info.GetReturnValue().Set(Nan::New(r));
}
