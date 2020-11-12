#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "SyntheticInput.h"
#include "ThreadPool.h"


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
        Nan::AsyncResource resource("erizo::addon.SyntheticInput.deleter");
        callback->Call(1, argv, &resource);
      }
    }
 private:
    std::shared_ptr<erizo::SyntheticInput> eiToDelete_;
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
  Nan::SetPrototypeMethod(tpl, "setFeedbackSource", setFeedbackSource);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("SyntheticInput").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(SyntheticInput::New) {
  if (info.Length() < 4) {
    Nan::ThrowError("Wrong number of arguments");
  }
  ThreadPool* thread_pool = Nan::ObjectWrap::Unwrap<ThreadPool>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

  uint32_t audio_bitrate = Nan::To<int>(info[1]).FromJust();
  uint32_t min_video_bitrate = Nan::To<int>(info[2]).FromJust();
  uint32_t max_video_bitrate = Nan::To<int>(info[3]).FromJust();

  std::shared_ptr<erizo::Worker> worker = thread_pool->me->getLessUsedWorker();

  erizo::SyntheticInputConfig config{audio_bitrate, min_video_bitrate, max_video_bitrate};
  SyntheticInput* obj = new SyntheticInput();
  obj->me = std::make_shared<erizo::SyntheticInput>(config, worker);

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

  me->start();
  info.GetReturnValue().Set(Nan::New(1));
}

NAN_METHOD(SyntheticInput::setAudioReceiver) {
  SyntheticInput* obj = ObjectWrap::Unwrap<SyntheticInput>(info.Holder());
  std::shared_ptr<erizo::SyntheticInput> me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

  me->setAudioSink(param->msink);
  me->setEventSink(param->msink);
}

NAN_METHOD(SyntheticInput::setVideoReceiver) {
  SyntheticInput* obj = ObjectWrap::Unwrap<SyntheticInput>(info.Holder());
  std::shared_ptr<erizo::SyntheticInput> me = obj->me;

  MediaSink* param = ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

  me->setVideoSink(param->msink);
  me->setEventSink(param->msink);
}

NAN_METHOD(SyntheticInput::setFeedbackSource) {
  SyntheticInput* obj = ObjectWrap::Unwrap<SyntheticInput>(info.Holder());
  std::shared_ptr<erizo::SyntheticInput> me = obj->me;

  MediaStream* param = ObjectWrap::Unwrap<MediaStream>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  std::shared_ptr<erizo::FeedbackSource> fb_source = param->me->getFeedbackSource().lock();

  if (fb_source) {
    fb_source->setFeedbackSink(me);
  }
}
