#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "OneToManyTranscoder.h"

using v8::Local;
using v8::Value;
using v8::Function;
using v8::HandleScope;

Nan::Persistent<Function> OneToManyTranscoder::constructor;

// Async Delete OTM

// Classes for Async (not in node main thread) operations
class AsyncDeleter : public Nan::AsyncWorker {
 public:
  AsyncDeleter(erizo::OneToManyTranscoder* ott, Nan::Callback *callback) :
    AsyncWorker(callback), ottToDelete_(ott) {
  }
  ~AsyncDeleter() {}
  void Execute() {
    delete ottToDelete_;
  }
  void HandleOKCallback() {
    HandleScope scope;
    std::string msg("OK");
    if (callback) {
      Local<Value> argv[] = {
        Nan::New(msg.c_str()).ToLocalChecked()
      };
      Nan::AsyncResource resource("erizo::addon.oneToManyTranscoder.deleter");
      callback->Call(1, argv), &resource;
    }
  }
 private:
  erizo::OneToManyTranscoder* ottToDelete_;
  Nan::Callback* callback_;
};

class AsyncRemoveSubscriber : public Nan::AsyncWorker{
 public:
  AsyncRemoveSubscriber(erizo::OneToManyTranscoder* ott, const std::string& peerId, Nan::Callback *callback):
    AsyncWorker(callback), ott_(ott), peerId_(peerId), callback_(callback) {
    }
  ~AsyncRemoveSubscriber() {}
  void Execute() {
    ott_->removeSubscriber(peerId_);
  }
  void HandleOKCallback() {
    // We're not doing anything here ATM
  }

 private:
  erizo::OneToManyTranscoder* ott_;
  std::string peerId_;
  Nan::Callback* callback_;
};


OneToManyTranscoder::OneToManyTranscoder() {}
OneToManyTranscoder::~OneToManyTranscoder() {}

NAN_MODULE_INIT(OneToManyTranscoder::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("OneToManyTranscoder").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "setPublisher", setPublisher);
  Nan::SetPrototypeMethod(tpl, "hasPublisher", hasPublisher);
  Nan::SetPrototypeMethod(tpl, "addSubscriber", addSubscriber);
  Nan::SetPrototypeMethod(tpl, "removeSubscriber", removeSubscriber);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("OneToManyTranscoder").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(OneToManyTranscoder::New) {
  OneToManyTranscoder* obj = new OneToManyTranscoder();
  obj->me = new erizo::OneToManyTranscoder();
  obj->msink = obj->me;

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(OneToManyTranscoder::close) {
  OneToManyTranscoder* obj = Nan::ObjectWrap::Unwrap<OneToManyTranscoder>(info.Holder());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;
  Nan::Callback *callback;
  if (info.Length() >= 1) {
    callback = new Nan::Callback(info[0].As<Function>());
  } else {
    callback = NULL;
  }

  Nan::AsyncQueueWorker(new  AsyncDeleter(me, callback));
}

NAN_METHOD(OneToManyTranscoder::setPublisher) {
  OneToManyTranscoder* obj = Nan::ObjectWrap::Unwrap<OneToManyTranscoder>(info.Holder());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  WebRtcConnection* param = Nan::ObjectWrap::Unwrap<WebRtcConnection>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)param->me;

  erizo::MediaSource* ms = dynamic_cast<erizo::MediaSource*>(wr);
  me->setPublisher(ms);
}

NAN_METHOD(OneToManyTranscoder::hasPublisher) {
  OneToManyTranscoder* obj = Nan::ObjectWrap::Unwrap<OneToManyTranscoder>(info.Holder());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  bool p = true;

  if (me->publisher == NULL) {
    p = false;
  }

  info.GetReturnValue().Set(Nan::New(p));
}

NAN_METHOD(OneToManyTranscoder::addSubscriber) {
  OneToManyTranscoder* obj = Nan::ObjectWrap::Unwrap<OneToManyTranscoder>(info.Holder());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  WebRtcConnection* param = Nan::ObjectWrap::Unwrap<WebRtcConnection>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)param->me;

  erizo::MediaSink* ms = dynamic_cast<erizo::MediaSink*>(wr);

  // get the param
  v8::String::Utf8Value param1(Nan::To<v8::String>(info[1]).ToLocalChecked());

  // convert it to string
  std::string peerId = std::string(*param1);
  me->addSubscriber(ms, peerId);
}

NAN_METHOD(OneToManyTranscoder::removeSubscriber) {
  OneToManyTranscoder* obj = Nan::ObjectWrap::Unwrap<OneToManyTranscoder>(info.Holder());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  // get the param
  v8::String::Utf8Value param1(Nan::To<v8::String>(info[0]).ToLocalChecked());

  // convert it to string
  std::string peerId = std::string(*param1);
  Nan::AsyncQueueWorker(new  AsyncRemoveSubscriber(me, peerId, NULL));
}
