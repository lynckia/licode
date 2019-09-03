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

DEFINE_LOGGER(MediaStream, "ErizoAPI.MediaStream");

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
  } else {
    stat_promise.set_value(std::string("{}"));
  }
  stat_future.wait();
  stat_ = stat_future.get();
}

void StatCallWorker::HandleOKCallback() {
  Local<Value> argv[] = {
    Nan::New<v8::String>(stat_).ToLocalChecked()
  };
  Nan::AsyncResource resource("erizo::addon.statCall");
  callback->Call(1, argv, &resource);
}

void destroyAsyncHandle(uv_handle_t *handle) {
  delete handle;
}

Nan::Persistent<Function> MediaStream::constructor;

MediaStream::MediaStream() : closed_{false}, id_{"undefined"} {
  async_stats_ = new uv_async_t;
  async_event_ = new uv_async_t;
  close_future_async_ = new uv_async_t;
  uv_async_init(uv_default_loop(), async_stats_, &MediaStream::statsCallback);
  uv_async_init(uv_default_loop(), async_event_, &MediaStream::eventCallback);
  uv_async_init(uv_default_loop(), close_future_async_, &MediaStream::closePromiseResolver);
}

MediaStream::~MediaStream() {
  boost::mutex::scoped_lock lock(mutex);
  closeEvents();
  ELOG_DEBUG("%s, message: Destroyed", toLog());
}

void MediaStream::closeEvents() {
  has_stats_callback_ = false;
  has_event_callback_ = false;
  if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(async_stats_))) {
    ELOG_DEBUG("%s, message: Closing Stats handle", toLog());
    uv_close(reinterpret_cast<uv_handle_t*>(async_stats_), destroyAsyncHandle);
  }
  async_stats_ = nullptr;
  if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(async_event_))) {
    ELOG_DEBUG("%s, message: Closing MediaStreamEvent handle", toLog());
    uv_close(reinterpret_cast<uv_handle_t*>(async_event_), destroyAsyncHandle);
  }
  async_event_ = nullptr;
  if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(close_future_async_))) {
    ELOG_DEBUG("%s, message: Closing future handle", toLog());
    uv_close(reinterpret_cast<uv_handle_t*>(close_future_async_), destroyAsyncHandle);
  }
  close_future_async_ = nullptr;
}

boost::future<void> MediaStream::close() {
  auto close_promise = std::make_shared<boost::promise<void>>();
  ELOG_DEBUG("%s, message: Trying to close", toLog());
  if (closed_) {
    ELOG_DEBUG("%s, message: Already closed", toLog());
    close_promise->set_value();
    return close_promise->get_future();
  }
  ELOG_DEBUG("%s, message: Closing", toLog());
  closed_ = true;

  if (me) {
    me->setMediaStreamStatsListener(nullptr);
    me->setMediaStreamEventListener(nullptr);

    me->close().then([this, close_promise] (boost::future<void>) {
        me.reset();
        close_promise->set_value();
    });
  } else {
    close_promise->set_value();
  }
  ELOG_DEBUG("%s, message: Closed", toLog());
  return close_promise->get_future();
}

std::string MediaStream::toLog() {
  return "id: " + id_;
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
  Nan::SetPrototypeMethod(tpl, "setMaxVideoBW", setMaxVideoBW);
  Nan::SetPrototypeMethod(tpl, "setQualityLayer", setQualityLayer);
  Nan::SetPrototypeMethod(tpl, "enableSlideShowBelowSpatialLayer", enableSlideShowBelowSpatialLayer);
  Nan::SetPrototypeMethod(tpl, "onMediaStreamEvent", onMediaStreamEvent);
  Nan::SetPrototypeMethod(tpl, "setVideoConstraints", setVideoConstraints);
  Nan::SetPrototypeMethod(tpl, "setMetadata", setMetadata);
  Nan::SetPrototypeMethod(tpl, "enableHandler", enableHandler);
  Nan::SetPrototypeMethod(tpl, "disableHandler", disableHandler);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("MediaStream").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}


NAN_METHOD(MediaStream::New) {
  if (info.Length() < 3) {
    Nan::ThrowError("Wrong number of arguments");
  }

  if (info.IsConstructCall()) {
    // Invoked as a constructor with 'new MediaStream()'
    ThreadPool* thread_pool = Nan::ObjectWrap::Unwrap<ThreadPool>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

    WebRtcConnection* connection =
     Nan::ObjectWrap::Unwrap<WebRtcConnection>(Nan::To<v8::Object>(info[1]).ToLocalChecked());

    std::shared_ptr<erizo::WebRtcConnection> wrtc = connection->me;

    v8::String::Utf8Value paramId(Nan::To<v8::String>(info[2]).ToLocalChecked());
    std::string wrtc_id = std::string(*paramId);

    v8::String::Utf8Value paramLabel(Nan::To<v8::String>(info[3]).ToLocalChecked());
    std::string stream_label = std::string(*paramLabel);

    bool is_publisher = info[5]->BooleanValue();

    std::shared_ptr<erizo::Worker> worker = thread_pool->me->getLessUsedWorker();

    MediaStream* obj = new MediaStream();
    obj->me = std::make_shared<erizo::MediaStream>(worker, wrtc, wrtc_id, stream_label, is_publisher);
    obj->msink = obj->me.get();
    obj->id_ = wrtc_id;
    obj->label_ = stream_label;
    ELOG_DEBUG("%s, message: Created", obj->toLog());
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    // TODO(pedro) Check what happens here
  }
}

NAN_METHOD(MediaStream::close) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(info.GetIsolate());
  Nan::Persistent<v8::Promise::Resolver> *persistent = new Nan::Persistent<v8::Promise::Resolver>(resolver);
  obj->Ref();
  obj->close().then(
      [persistent, obj] (boost::future<void>) {
        ELOG_DEBUG("%s, MediaStream Close is finishied, resolving promise", obj->toLog());
        obj->notifyFuture(persistent);
      });
  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_METHOD(MediaStream::init) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }
  bool force =  info.Length() > 0 ? info[0]->BooleanValue() : false;
  bool r = me->init(force);

  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(MediaStream::setSlideShowMode) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool v = info[0]->BooleanValue();
  me->setSlideShowMode(v);
  info.GetReturnValue().Set(Nan::New(v));
}

NAN_METHOD(MediaStream::muteStream) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool mute_video = info[0]->BooleanValue();
  bool mute_audio = info[1]->BooleanValue();
  me->muteStream(mute_video, mute_audio);
}

NAN_METHOD(MediaStream::setMaxVideoBW) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  int max_video_bw = info[0]->IntegerValue();
  me->setMaxVideoBW(max_video_bw);
}

NAN_METHOD(MediaStream::setVideoConstraints) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }
  int max_video_width = info[0]->IntegerValue();
  int max_video_height = info[1]->IntegerValue();
  int max_video_frame_rate = info[2]->IntegerValue();
  me->setVideoConstraints(max_video_width, max_video_height, max_video_frame_rate);
}

NAN_METHOD(MediaStream::setMetadata) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

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
  if (!me || obj->closed_) {
    return;
  }

  int state = me->getCurrentState();

  info.GetReturnValue().Set(Nan::New(state));
}


NAN_METHOD(MediaStream::setAudioReceiver) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  MediaSink* param = Nan::ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setAudioSink(mr);
  me->setEventSink(mr);
}

NAN_METHOD(MediaStream::setVideoReceiver) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  MediaSink* param = Nan::ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::MediaSink *mr = param->msink;

  me->setVideoSink(mr);
  me->setEventSink(mr);
}


NAN_METHOD(MediaStream::generatePLIPacket) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());

  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }
  me->sendPLI();
}

NAN_METHOD(MediaStream::enableHandler) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string name = std::string(*param);

  me->enableHandler(name);
  return;
}

NAN_METHOD(MediaStream::disableHandler) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  v8::String::Utf8Value param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string name = std::string(*param);

  me->disableHandler(name);
}

NAN_METHOD(MediaStream::setQualityLayer) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  int spatial_layer = info[0]->IntegerValue();
  int temporal_layer = info[1]->IntegerValue();

  me->setQualityLayer(spatial_layer, temporal_layer);
}

NAN_METHOD(MediaStream::enableSlideShowBelowSpatialLayer) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool enabled = info[0]->BooleanValue();
  int spatial_layer = info[1]->IntegerValue();
  me->enableSlideShowBelowSpatialLayer(enabled, spatial_layer);
}

NAN_METHOD(MediaStream::getStats) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
  if (!obj->me || info.Length() != 1 || obj->closed_) {
    Local<Value> argv[] = {
      Nan::New<v8::String>("{}").ToLocalChecked()
    };
    Nan::Call(*callback, 1, argv);
    return;
  }
  AsyncQueueWorker(new StatCallWorker(callback, obj->me));
}

NAN_METHOD(MediaStream::getPeriodicStats) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  if (!obj->me || info.Length() != 1 || obj->closed_) {
    return;
  }
  obj->me->setMediaStreamStatsListener(obj);
  obj->has_stats_callback_ = true;
  obj->stats_callback_ = new Nan::Callback(info[0].As<Function>());
}

NAN_METHOD(MediaStream::setFeedbackReports) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool v = info[0]->BooleanValue();
  int fbreps = info[1]->IntegerValue();  // From bps to Kbps
  me->setFeedbackReports(v, fbreps);
}

NAN_METHOD(MediaStream::onMediaStreamEvent) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }
  me ->setMediaStreamEventListener(obj);
  obj->has_event_callback_ = true;
  obj->event_callback_ = new Nan::Callback(info[0].As<Function>());
}

void MediaStream::notifyStats(const std::string& message) {
  boost::mutex::scoped_lock lock(mutex);
  if (!has_stats_callback_) {
    return;
  }
  if (!async_stats_) {
    return;
  }
  stats_messages.push(message);
  async_stats_->data = this;
  uv_async_send(async_stats_);
}

void MediaStream::notifyMediaStreamEvent(const std::string& type, const std::string& message) {
  boost::mutex::scoped_lock lock(mutex);
  if (!has_event_callback_) {
    return;
  }
  if (!async_event_) {
    return;
  }
  event_messages.push(std::make_pair(type, message));
  async_event_->data = this;
  uv_async_send(async_event_);
}

NAUV_WORK_CB(MediaStream::statsCallback) {
  Nan::HandleScope scope;
  MediaStream* obj = reinterpret_cast<MediaStream*>(async->data);
  if (!obj || !obj->me || obj->closed_) {
    return;
  }
  boost::mutex::scoped_lock lock(obj->mutex);
  if (obj->has_stats_callback_) {
    while (!obj->stats_messages.empty()) {
      Local<Value> args[] = {Nan::New(obj->stats_messages.front().c_str()).ToLocalChecked()};
      Nan::AsyncResource resource("erizo::addon.stream.statsCallback");
      resource.runInAsyncScope(Nan::GetCurrentContext()->Global(), obj->stats_callback_->GetFunction(), 1, args);
      obj->stats_messages.pop();
    }
  }
}

NAUV_WORK_CB(MediaStream::eventCallback) {
  Nan::HandleScope scope;
  MediaStream* obj = reinterpret_cast<MediaStream*>(async->data);
  if (!obj || !obj->me || obj->closed_) {
    return;
  }
  boost::mutex::scoped_lock lock(obj->mutex);
  ELOG_DEBUG("%s, message: eventsCallback", obj->toLog());
  if (obj->has_event_callback_) {
      while (!obj->event_messages.empty()) {
          Local<Value> args[] = {Nan::New(obj->event_messages.front().first.c_str()).ToLocalChecked(),
              Nan::New(obj->event_messages.front().second.c_str()).ToLocalChecked()};
          Nan::AsyncResource resource("erizo::addon.stream.eventCallback");
          resource.runInAsyncScope(Nan::GetCurrentContext()->Global(), obj->event_callback_->GetFunction(), 2, args);
          obj->event_messages.pop();
      }
  }
  ELOG_DEBUG("%s, message: eventsCallback finished", obj->toLog());
}


void MediaStream::notifyFuture(Nan::Persistent<v8::Promise::Resolver> *persistent) {
  boost::mutex::scoped_lock lock(mutex);
  if (!close_future_async_) {
    return;
  }
  futures.push(persistent);
  close_future_async_->data = this;
  uv_async_send(close_future_async_);
}

NAUV_WORK_CB(MediaStream::closePromiseResolver) {
  Nan::HandleScope scope;
  MediaStream* obj = reinterpret_cast<MediaStream*>(async->data);
  if (!obj) {  // closed_ will always be true here
    return;
  }
  boost::mutex::scoped_lock lock(obj->mutex);
  ELOG_DEBUG("%s, message: closePromiseResolver", obj->toLog());
  obj->futures_manager_.cleanResolvedFutures();
  obj->Ref();
  while (!obj->futures.empty()) {
    auto persistent = obj->futures.front();
    v8::Local<v8::Promise::Resolver> resolver = Nan::New(*persistent);
    resolver->Resolve(Nan::GetCurrentContext(), Nan::New("").ToLocalChecked());
    obj->futures.pop();
    obj->Unref();
  }
  obj->closeEvents();
  obj->Unref();
  ELOG_DEBUG("%s, message: closePromiseResolver finished", obj->toLog());
}
