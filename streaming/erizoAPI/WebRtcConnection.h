#ifndef WEBRTCCONNECTION_H
#define WEBRTCCONNECTION_H

#include <node.h>
#include <WebRtcConnection.h>
#include "MediaReceiver.h"
#include "OneToManyProcessor.h"

class WebRtcConnection : public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> target);

  erizo::WebRtcConnection *me;

 private:
  WebRtcConnection();
  ~WebRtcConnection();

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> init(const v8::Arguments& args);
  static v8::Handle<v8::Value> close(const v8::Arguments& args);
  static v8::Handle<v8::Value> setRemoteSdp(const v8::Arguments& args);
  static v8::Handle<v8::Value> getLocalSdp(const v8::Arguments& args);
  static v8::Handle<v8::Value> setAudioReceiver(const v8::Arguments& args);
  static v8::Handle<v8::Value> setVideoReceiver(const v8::Arguments& args);
 
};

#endif