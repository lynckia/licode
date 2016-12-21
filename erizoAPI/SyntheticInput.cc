#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "SyntheticInput.h"


using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Value;


Nan::Persistent<Function> SyntheticInput::constructor;

class AsyncDeleter : public Nan::AsyncWorker {
 public:
    AsyncDeleter(std::shared_ptr<erizo::SyntheticInput> eiToDelete, Nan::Callback *callback):
      AsyncWorker(callback), eiToDelete_(eiToDelete) {
      }
    ~AsyncDeleter() {}
    void Execute() {
      eiToDelete_.reset();
    }
    void HandleOKCallback() {
      Nan::HandleScope scope;
      std::string msg("OK");
      if (callback) {
        Local<Value> argv[] = {
          Nan::New(msg.c_str()).ToLocalChecked()
        };
        callback->Call(1, argv);
      }
    }
 private:
    std::shared_ptr<erizo::SyntheticInput> eiToDelete_;
    Nan::Callback* callback_;
};

SyntheticInput::SyntheticInput() {}
SyntheticInput::~SyntheticInput() {}

NAN_MODULE_INIT(SyntheticInput::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("SyntheticInput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);
  Nan::SetPrototypeMethod(tpl, "setAudioReceiver", setAudioReceiver);
  Nan::SetPrototypeMethod(tpl, "setVideoReceiver", setVideoReceiver);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("SyntheticInput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(SyntheticInput::New) {
  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);

  SyntheticInput* obj = new SyntheticInput();
  obj->me = std::make_shared<erizo::SyntheticInput>(url);

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(SyntheticInput::close) {
  SyntheticInput* obj = ObjectWrap::Unwrap<SyntheticInput>(info.Holder());
  std::shared_ptr<erizo::SyntheticInput> me = obj->me;

  Nan::Callback *callback;
  if (info.Length() >= 1) {
    callback = new Nan::Callback(info[0].As<Function>());
  } else {
    callback = NULL;
  }

  Nan::AsyncQueueWorker(new  AsyncDeleter(me, callback));
  me.reset();
}

NAN_METHOD(SyntheticInput::init) {
  SyntheticInput* obj = ObjectWrap::Unwrap<SyntheticInput>(info.Holder());
  std::shared_ptr<erizo::SyntheticInput> me = obj->me;

  int r = me->start();
  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(SyntheticInput::setAudioReceiver) {
  SyntheticInput* obj = ObjectWrap::Unwrap<SyntheticInput>(info.Holder());
  std::shared_ptr<erizo::SyntheticInput> me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setAudioSink(mr);
}

NAN_METHOD(SyntheticInput::setVideoReceiver) {
  SyntheticInput* obj = ObjectWrap::Unwrap<SyntheticInput>(info.Holder());
  std::shared_ptr<erizo::SyntheticInput> me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setVideoSink(mr);
}
