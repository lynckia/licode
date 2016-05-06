#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "ExternalInput.h"


using namespace v8;

Nan::Persistent<Function> ExternalInput::constructor;

class AsyncDeleter : public Nan::AsyncWorker {
  public:
    AsyncDeleter (erizo::ExternalInput* eiToDelete, Nan::Callback *callback):
      AsyncWorker(callback), eiToDelete_(eiToDelete){
      }
    ~AsyncDeleter() {}
    void Execute(){
      delete eiToDelete_;
    }
    void HandleOKCallback() {
      HandleScope scope;
      std::string msg("OK");
      if (callback){
        Local<Value> argv[] = {
          Nan::New(msg.c_str()).ToLocalChecked()
        };
        callback->Call(1, argv);
      }
    }
  private:
    erizo::ExternalInput* eiToDelete_;
    Nan::Callback* callback_;
};

ExternalInput::ExternalInput() {};
ExternalInput::~ExternalInput() {};

NAN_MODULE_INIT (ExternalInput::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ExternalInput").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);
  Nan::SetPrototypeMethod(tpl, "setAudioReceiver", setAudioReceiver);
  Nan::SetPrototypeMethod(tpl, "setVideoReceiver", setVideoReceiver);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("ExternalInput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ExternalInput::New) {
  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string url = std::string(*param);

  ExternalInput* obj = new ExternalInput();
  obj->me = new erizo::ExternalInput(url);

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ExternalInput::close) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  Nan::Callback *callback; 
  if (info.Length()>=1){
    callback =new Nan::Callback(info[0].As<Function>());
  } else {
    callback = NULL;
  }

  Nan::AsyncQueueWorker(new  AsyncDeleter(me, callback));
}

NAN_METHOD(ExternalInput::init) {
  //TODO:Could potentially be slow, think about async'ing it  
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  int r = me->init();
  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(ExternalInput::setAudioReceiver) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setAudioSink(mr);
}

NAN_METHOD(ExternalInput::setVideoReceiver) {
  ExternalInput* obj = ObjectWrap::Unwrap<ExternalInput>(info.Holder());
  erizo::ExternalInput *me = (erizo::ExternalInput*)obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setVideoSink(mr);
}
