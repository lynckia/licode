#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "MediaStream.h"

#include <future>  // NOLINT

#include "lib/json.hpp"
#include "ThreadPool.h"
#include <HandlerImporter.h>

using v8::HandleScope;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Persistent;
using v8::Exception;
using v8::Value;
using v8::Array;
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

void MediaStream::computePromiseTimes(erizo::time_point promise_start,
    erizo::time_point promise_resolved, erizo::time_point promise_notified) {
  promise_durations_.add(promise_resolved - promise_start);
  promise_delays_.add(promise_notified - promise_resolved);
}


NAN_MODULE_INIT(MediaStream::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("MediaStream").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "configure", configure);
  Nan::SetPrototypeMethod(tpl, "setAudioReceiver", setAudioReceiver);
  Nan::SetPrototypeMethod(tpl, "setVideoReceiver", setVideoReceiver);
  Nan::SetPrototypeMethod(tpl, "getCurrentState", getCurrentState);
  Nan::SetPrototypeMethod(tpl, "generatePLIPacket", generatePLIPacket);
  Nan::SetPrototypeMethod(tpl, "getStats", getStats);
  Nan::SetPrototypeMethod(tpl, "getPeriodicStats", getPeriodicStats);
  Nan::SetPrototypeMethod(tpl, "setFeedbackReports", setFeedbackReports);
  Nan::SetPrototypeMethod(tpl, "setSlideShowMode", setSlideShowMode);
  Nan::SetPrototypeMethod(tpl, "setPriority", setPriority);
  Nan::SetPrototypeMethod(tpl, "muteStream", muteStream);
  Nan::SetPrototypeMethod(tpl, "setMaxVideoBW", setMaxVideoBW);
  Nan::SetPrototypeMethod(tpl, "setQualityLayer", setQualityLayer);
  Nan::SetPrototypeMethod(tpl, "enableSlideShowBelowSpatialLayer", enableSlideShowBelowSpatialLayer);
  Nan::SetPrototypeMethod(tpl, "onMediaStreamEvent", onMediaStreamEvent);
  Nan::SetPrototypeMethod(tpl, "setVideoConstraints", setVideoConstraints);
  Nan::SetPrototypeMethod(tpl, "setMetadata", setMetadata);
  Nan::SetPrototypeMethod(tpl, "setPeriodicKeyframeRequests", setPeriodicKeyframeRequests);
  Nan::SetPrototypeMethod(tpl, "hasPeriodicKeyframeRequests", hasPeriodicKeyframeRequests);
  Nan::SetPrototypeMethod(tpl, "enableHandler", enableHandler);
  Nan::SetPrototypeMethod(tpl, "disableHandler", disableHandler);
  Nan::SetPrototypeMethod(tpl, "getDurationDistribution", getDurationDistribution);
  Nan::SetPrototypeMethod(tpl, "getDelayDistribution", getDelayDistribution);
  Nan::SetPrototypeMethod(tpl, "resetStats", resetStats);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
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

    Nan::Utf8String paramId(Nan::To<v8::String>(info[2]).ToLocalChecked());
    std::string wrtc_id = std::string(*paramId);

    Nan::Utf8String paramLabel(Nan::To<v8::String>(info[3]).ToLocalChecked());
    std::string stream_label = std::string(*paramLabel);

    bool is_publisher = Nan::To<bool>(info[4]).FromJust();
    bool has_audio = Nan::To<bool>(info[5]).FromJust();
    bool has_video = Nan::To<bool>(info[6]).FromJust();
    std::shared_ptr<erizo::Worker> worker = thread_pool->me->getLessUsedWorker();

    std::vector<std::string> handler_order = {};
    std::map<std::string, std::shared_ptr<erizo::CustomHandler>> handlers_pointer_dic = {};
    if (info.Length() > 7) {
      std::vector<std::map<std::string, std::string>> custom_handlers = {};
      Local<Array> jsArr = Local<Array>::Cast(info[7]);
      for (unsigned int i = 0; i < jsArr->Length(); i++) {
          Nan::Utf8String js_element(Nan::To<v8::String>(Nan::Get(jsArr, i).ToLocalChecked()).ToLocalChecked());
          std::string parameters = std::string(*js_element);
          nlohmann::json handlers_json = nlohmann::json::parse(parameters);
          std::map<std::string, std::string> params_dic = {};
          for (json::iterator it = handlers_json.begin(); it != handlers_json.end(); ++it) {
            params_dic.insert((std::pair<std::string, std::string>(it.key(), it.value())));
          }
          custom_handlers.push_back(params_dic);
      }
      erizo::HandlerImporter importer;

      importer.loadHandlers(custom_handlers);
      handler_order = importer.handler_order;
      handlers_pointer_dic = importer.handlers_pointer_dic;
    }

    std::string priority = "default";
    if (info.Length() > 8) {
      Nan::Utf8String paramPriority(Nan::To<v8::String>(info[8]).ToLocalChecked());
      priority = std::string(*paramPriority);
      if (priority == "undefined" || priority == "") {
        priority = "default";
      }
    }

    MediaStream* obj = new MediaStream();
    obj->me = std::make_shared<erizo::MediaStream>(worker, wrtc, wrtc_id, stream_label, is_publisher,
      has_audio, has_video, priority, handler_order, handlers_pointer_dic);
    obj->me->init();
    obj->msink = obj->me;
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
  v8::Local<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(Nan::GetCurrentContext()).ToLocalChecked();
  Nan::Persistent<v8::Promise::Resolver> *persistent = new Nan::Persistent<v8::Promise::Resolver>(resolver);
  obj->Ref();
  erizo::time_point promise_start = erizo::clock::now();
  obj->close().then(
      [persistent, obj, promise_start] (boost::future<void>) {
        ELOG_DEBUG("%s, MediaStream Close is finished, resolving promise", obj->toLog());
        obj->notifyFuture(persistent, promise_start);
      });
  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_METHOD(MediaStream::configure) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }
  bool force =  info.Length() > 0 ? Nan::To<bool>(info[0]).FromJust() : false;
  bool r = me->configure(force);

  info.GetReturnValue().Set(Nan::New(r));
}

NAN_METHOD(MediaStream::setSlideShowMode) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool v = Nan::To<bool>(info[0]).FromJust();
  me->setSlideShowMode(v);
  info.GetReturnValue().Set(Nan::New(v));
}

NAN_METHOD(MediaStream::setPriority) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }
  Nan::Utf8String param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string priority = std::string(*param);

  me->setPriority(priority);
}

NAN_METHOD(MediaStream::muteStream) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool mute_video = Nan::To<bool>(info[0]).FromJust();
  bool mute_audio = Nan::To<bool>(info[1]).FromJust();
  me->muteStream(mute_video, mute_audio);
}

NAN_METHOD(MediaStream::setMaxVideoBW) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  int max_video_bw = Nan::To<int>(info[0]).FromJust();
  me->setMaxVideoBW(max_video_bw);
}

NAN_METHOD(MediaStream::setVideoConstraints) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }
  int max_video_width = Nan::To<int>(info[0]).FromJust();
  int max_video_height = Nan::To<int>(info[1]).FromJust();
  int max_video_frame_rate = Nan::To<int>(info[2]).FromJust();
  me->setVideoConstraints(max_video_width, max_video_height, max_video_frame_rate);
}

NAN_METHOD(MediaStream::setMetadata) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  Nan::Utf8String json_param(Nan::To<v8::String>(info[0]).ToLocalChecked());
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

  me->setAudioSink(param->msink);
  me->setEventSink(param->msink);
}

NAN_METHOD(MediaStream::setVideoReceiver) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  MediaSink* param = Nan::ObjectWrap::Unwrap<MediaSink>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

  me->setVideoSink(param->msink);
  me->setEventSink(param->msink);
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

  Nan::Utf8String param(Nan::To<v8::String>(info[0]).ToLocalChecked());
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

  Nan::Utf8String param(Nan::To<v8::String>(info[0]).ToLocalChecked());
  std::string name = std::string(*param);

  me->disableHandler(name);
}

NAN_METHOD(MediaStream::setQualityLayer) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  int spatial_layer = Nan::To<int>(info[0]).FromJust();
  int temporal_layer = Nan::To<int>(info[1]).FromJust();

  me->setQualityLayer(spatial_layer, temporal_layer);
}

NAN_METHOD(MediaStream::enableSlideShowBelowSpatialLayer) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool enabled = Nan::To<bool>(info[0]).FromJust();
  int spatial_layer = Nan::To<int>(info[1]).FromJust();
  me->enableSlideShowBelowSpatialLayer(enabled, spatial_layer);
}

NAN_METHOD(MediaStream::setPeriodicKeyframeRequests) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool activated = Nan::To<bool>(info[0]).FromJust();
  int interval = 0;
  if (info.Length() > 1) {
    interval = Nan::To<int>(info[1]).FromJust();
  }
  me->setPeriodicKeyframeRequests(activated, interval);
}

NAN_METHOD(MediaStream::hasPeriodicKeyframeRequests) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  std::shared_ptr<erizo::MediaStream> me = obj->me;
  if (!me || obj->closed_) {
    return;
  }

  bool has_periodic_requests = me->isRequestingPeriodicKeyframes();
  info.GetReturnValue().Set(Nan::New(has_periodic_requests));
}

NAN_METHOD(MediaStream::getDurationDistribution) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  PromiseDurationDistribution duration_distribution = obj->promise_durations_;
  v8::Local<v8::Array> array = Nan::New<v8::Array>(5);
  Nan::Set(array, 0, Nan::New(duration_distribution.duration_0_10_ms));
  Nan::Set(array, 1, Nan::New(duration_distribution.duration_10_50_ms));
  Nan::Set(array, 2, Nan::New(duration_distribution.duration_50_100_ms));
  Nan::Set(array, 3, Nan::New(duration_distribution.duration_100_1000_ms));
  Nan::Set(array, 4, Nan::New(duration_distribution.duration_1000_ms));

  info.GetReturnValue().Set(array);
}

NAN_METHOD(MediaStream::getDelayDistribution) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  PromiseDurationDistribution duration_distribution = obj->promise_delays_;
  v8::Local<v8::Array> array = Nan::New<v8::Array>(5);
  Nan::Set(array, 0, Nan::New(duration_distribution.duration_0_10_ms));
  Nan::Set(array, 1, Nan::New(duration_distribution.duration_10_50_ms));
  Nan::Set(array, 2, Nan::New(duration_distribution.duration_50_100_ms));
  Nan::Set(array, 3, Nan::New(duration_distribution.duration_100_1000_ms));
  Nan::Set(array, 4, Nan::New(duration_distribution.duration_1000_ms));

  info.GetReturnValue().Set(array);
}

NAN_METHOD(MediaStream::resetStats) {
  MediaStream* obj = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  obj->promise_durations_.reset();
  obj->promise_delays_.reset();
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

  bool v = Nan::To<bool>(info[0]).FromJust();
  int fbreps = Nan::To<int>(info[1]).FromJust();  // From bps to Kbps
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


void MediaStream::notifyFuture(Nan::Persistent<v8::Promise::Resolver> *persistent, erizo::time_point promise_start) {
  boost::mutex::scoped_lock lock(mutex);
  if (!close_future_async_) {
    return;
  }
  StreamResultTuple result_tuple(persistent, promise_start, erizo::clock::now());
  futures.push(result_tuple);
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
  obj->Ref();
  while (!obj->futures.empty()) {
    auto persistent = std::get<0>(obj->futures.front());
    v8::Local<v8::Promise::Resolver> resolver = Nan::New(*persistent);
    erizo::time_point promise_start = std::get<1>(obj->futures.front());
    erizo::time_point promise_resolved = std::get<2>(obj->futures.front());
    erizo::time_point promise_notified = erizo::clock::now();
    obj->computePromiseTimes(promise_start, promise_resolved, promise_notified);

    resolver->Resolve(Nan::GetCurrentContext(), Nan::New("").ToLocalChecked()).IsNothing();
    persistent->Reset();
    delete persistent;
    obj->futures.pop();
    obj->Unref();
    v8::Isolate::GetCurrent()->RunMicrotasks();
  }
  obj->closeEvents();
  obj->Unref();
  ELOG_DEBUG("%s, message: closePromiseResolver finished", obj->toLog());
}
