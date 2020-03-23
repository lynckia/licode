#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "ExternalInput.h"


using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Value;


Nan::Persistent<Function> ExternalInput::constructor;

class AsyncDeleter : public Nan::AsyncWorker {
 public:
    AsyncDeleter(std::shared_ptr<erizo::ExternalInput> eiToDelete, Nan::Callback *callback):
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
        Nan::AsyncResource resource("erizo::addon.externalInput.deleter");
        callback->Call(1, argv, &resource);
      }
    }
 private:
    std::shared_ptr<erizo::ExternalInput> eiToDelete_;
};

ExternalInput::ExternalInput() {}
ExternalInput::~ExternalInput() {}

NAN_MODULE_INIT(ExternalInput::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ExternalInput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);
  Nan::SetPrototypeMethod(tpl, "setAudioReceiver", setAudioReceiver);
  Nan::SetPrototypeMethod(tpl, "setVideoReceiver", setVideoReceiver);
  Nan::SetPrototypeMethod(tpl, "generatePLIPacket", generatePLIPacket);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("ExternalInput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ExternalInput::New) {
  Nan::Utf8String param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);

  ExternalInput* obj = new ExternalInput();
  obj->me = std::make_shared<erizo::ExternalInput>(url);

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ExternalInput::close) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  std::shared_ptr<erizo::ExternalInput> me = obj->me;

  Nan::Callback *callback;
  if (info.Length() >= 1) {
    callback = new Nan::Callback(info[0].As<Function>());
  } else {
    callback = NULL;
  }

  Nan::AsyncQueueWorker(new  AsyncDeleter(me, callback));
  obj->me.reset();
}

NAN_METHOD(ExternalInput::init) {
  // TODO(pedro) Could potentially be slow, think about async'ing it
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  std::shared_ptr<erizo::ExternalInput> me = obj->me;

  int r = me->init();
  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(ExternalInput::setAudioReceiver) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  std::shared_ptr<erizo::ExternalInput> me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

  me->setAudioSink(param->msink);
  me->setEventSink(param->msink);
}

NAN_METHOD(ExternalInput::setVideoReceiver) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  std::shared_ptr<erizo::ExternalInput> me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

  me->setVideoSink(param->msink);
  me->setEventSink(param->msink);
}

NAN_METHOD(ExternalInput::generatePLIPacket) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  std::shared_ptr<erizo::ExternalInput> me = obj->me;

  if (!me) {
    return;
  }
  me->sendPLI();
}
