#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "MediaStream.h"

#include <future>  // NOLINT

#include "lib/json.hpp"
#include "ThreadPool.h"

using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Persistent;
using v8::Exception;
using v8::Value;
using json = nlohmann::json;

StatCallWorker::StatCallWorker(Nan::Callback *callback, std::weak_ptr<erizo::MediaStream> weak_stream)
    : Nan::AsyncWorker{callback}, weak_stream_{weak_stream}, stat_{""} {
}

void StatCallWorker::Execute() {
  std::promise<std::string> stat_promise;
  std::future<std::string> stat_future = stat_promise.get_future();
  if (auto stream = weak_stream_.lock()) {
    stream->getJSONStats([&stat_promise] (std::string stats) {
      stat_promise.set_value(stats);
    });
  }
  stat_future.wait();
  stat_ = stat_future.get();
}

void StatCallWorker::HandleOKCallback() {
  Local<Value> argv[] = {
    Nan::New<v8::String>(stat_).ToLocalChecked()
  };
  callback->Call(1, argv);
}

Nan::Persistent<Function> MediaStream::constructor;

MediaStream::MediaStream() {
}

MediaStream::~MediaStream() {
}

NAN_MODULE_INIT(MediaStream::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("MediaStream").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "init", init);
  Nan::SetPrototypeMethod(tpl, "setAudioReceiver", setAudioReceiver);
  Nan::SetPrototypeMethod(tpl, "setVideoReceiver", setVideoReceiver);
  Nan::SetPrototypeMethod(tpl, "getCurrentState", getCurrentState);
  Nan::SetPrototypeMethod(tpl, "generatePLIPacket", generatePLIPacket);
  Nan::SetPrototypeMethod(tpl, "getStats", getStats);
  Nan::SetPrototypeMethod(tpl, "getPeriodicStats", getPeriodicStats);
  Nan::SetPrototypeMethod(tpl, "setFeedbackReports", setFeedbackReports);
  Nan::SetPrototypeMethod(tpl, "setSlideShowMode", setSlideShowMode);
  Nan::SetPrototypeMethod(tpl, "muteStream", muteStream);
  Nan::SetPrototypeMethod(tpl, "setQualityLayer", setQualityLayer);
  Nan::SetPrototypeMethod(tpl, "setVideoConstraints", setVideoConstraints);
  Nan::SetPrototypeMethod(tpl, "setMetadata", setMetadata);
  Nan::SetPrototypeMethod(tpl, "enableHandler", enableHandler);
  Nan::SetPrototypeMethod(tpl, "disableHandler", disableHandler);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("MediaStream").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}


NAN_METHOD(MediaStream::New) {
  if (info.Length() < 2) {
    Nan::ThrowError("Wrong number of arguments");
  }

  if (info.IsConstructCall()) {
    // Invoked as a constructor with 'new MediaStream()'
    WebRtcConnection* connection =
     Nan::ObjectWrap::Unwrap<WebRtcConnection>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

    std::shared_ptr<erizo::WebRtcConnection> wrtc = connection->me;

    v8::String::Utf8Value paramId(Nan::To<v8::String>(info[1]).ToLocalChecked());
    std::string wrtcId = std::string(*paramId);
    MediaStream* obj = new MediaStream();
    obj->me = std::make_shared<erizo::MediaStream>(wrtc, wrtcId);
    obj->msink = obj->me.get();
    uv_async_init(uv_default_loop(), &obj->asyncStats_, &MediaStream::statsCallback);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // TODO(pedro) Check what happens here
  }
}

NAN_METHOD(MediaStream::close) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  obj->me->setMediaStreamStatsListener(NULL);
  obj->hasCallback_ = false;
  if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&obj->asyncStats_))) {
    uv_close(reinterpret_cast<uv_handle_t*>(&obj->asyncStats_), NULL);
  }
  obj->me.reset();
}

NAN_METHOD(MediaStream::init) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  bool r = me->init();

  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(MediaStream::setSlideShowMode) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  bool v = info[0]->BooleanValue();
  me->setSlideShowMode(v);
  info.GetReturnValue().Set(Nan::New(v));
}

NAN_METHOD(MediaStream::muteStream) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  bool mute_video = info[0]->BooleanValue();
  bool mute_audio = info[1]->BooleanValue();
  me->muteStream(mute_video, mute_audio);
}

NAN_METHOD(MediaStream::setVideoConstraints) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  int max_video_width = info[0]->IntegerValue();
  int max_video_height = info[1]->IntegerValue();
  int max_video_frame_rate = info[2]->IntegerValue();
  me->setVideoConstraints(max_video_width, max_video_height, max_video_frame_rate);
}

NAN_METHOD(MediaStream::setMetadata) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  v8::String::Utf8Value json_param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string metadata_string = std::string(*json_param);
  json metadata_json = json::parse(metadata_string);
  std::map<std::string, std::string> metadata;
  for (json::iterator item = metadata_json.begin(); item != metadata_json.end(); ++item) {
    std::string value = item.value().dump();
    if (item.value().is_object()) {
      value = "[object]";
    }
    if (item.value().is_string()) {
      value = item.value();
    }
    metadata[item.key()] = value;
  }

  me->setMetadata(metadata);

  info.GetReturnValue().Set(Nan::New(true));
}


NAN_METHOD(MediaStream::getCurrentState) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  int state = me->getCurrentState();

  info.GetReturnValue().Set(Nan::New(state));
}


NAN_METHOD(MediaStream::setAudioReceiver) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  MediaSink* param = Nan::ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setAudioSink(mr);
  me->setEventSink(mr);
}

NAN_METHOD(MediaStream::setVideoReceiver) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  MediaSink* param = Nan::ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setVideoSink(mr);
  me->setEventSink(mr);
}


NAN_METHOD(MediaStream::generatePLIPacket) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());

  if (obj->me == NULL) {
    return;
  }

  std::shared_ptr<erizo::MediaStream> me = obj->me;
  me->sendPLI();
  return;
}

NAN_METHOD(MediaStream::enableHandler) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string name = std::string(*param);

  me->enableHandler(name);
  return;
}

NAN_METHOD(MediaStream::disableHandler) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string name = std::string(*param);

  me->disableHandler(name);
  return;
}

NAN_METHOD(MediaStream::setQualityLayer) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  int spatial_layer = info[0]->IntegerValue();
  int temporal_layer = info[1]->IntegerValue();

  me->setQualityLayer(spatial_layer, temporal_layer);

  return;
}

NAN_METHOD(MediaStream::getStats) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  if (!obj->me || info.Length() != 1) {
    return;
  }
  Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
  AsyncQueueWorker(new StatCallWorker(callback, obj->me));
}

NAN_METHOD(MediaStream::getPeriodicStats) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  if (obj->me == nullptr || info.Length() != 1) {
    return;
  }
  obj->me->setMediaStreamStatsListener(obj);
  obj->hasCallback_ = true;
  obj->statsCallback_ = new Nan::Callback(info[0].As<Function>());
}

NAN_METHOD(MediaStream::setFeedbackReports) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;

  bool v = info[0]->BooleanValue();
  int fbreps = info[1]->IntegerValue();  // From bps to Kbps
  me->setFeedbackReports(v, fbreps);
}

void MediaStream::notifyStats(const std::string& message) {
  if (!this->hasCallback_) {
    return;
  }
  boost::mutex::scoped_lock lock(mutex);
  this->statsMsgs.push(message);
  asyncStats_.data = this;
  uv_async_send(&asyncStats_);
}

NAUV_WORK_CB(MediaStream::statsCallback) {
  Nan::HandleScope scope;
  MediaStream* obj = reinterpret_cast<MediaStream*>(async->data);
  if (!obj || obj->me == NULL)
    return;
  boost::mutex::scoped_lock lock(obj->mutex);
  if (obj->hasCallback_) {
    while (!obj->statsMsgs.empty()) {
      Local<Value> args[] = {Nan::New(obj->statsMsgs.front().c_str()).ToLocalChecked()};
      Nan::MakeCallback(Nan::GetCurrentContext()->Global(), obj->statsCallback_->GetFunction(), 1, args);
      obj->statsMsgs.pop();
    }
  }
}
