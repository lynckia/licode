#ifndef EXTERNALINPUT_H
#define EXTERNALINPUT_H

#include <nan.h>
#include <media/ExternalInput.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"


/*
 * Wrapper class of erizo::ExternalInput
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class ExternalInput: public MediaSource {
 public:
  static void Init(v8::Local<v8::Object> exports);
  erizo::ExternalInput* me;

 private:
  ExternalInput();
  ~ExternalInput();

  /*
   * Constructor.
   * Constructs a ExternalInput
   */
  static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Closes the ExternalInput.
   * The object cannot be used after this call
   */
  static void close(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Inits the ExternalInput 
   * Returns true ready
   */
  static void init(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Sets a MediaSink that is going to receive Audio Data
   * Param: the MediaSink to send audio to.
   */
  static void setAudioReceiver(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Sets a MediaSink that is going to receive Video Data
   * Param: the MediaSink
   */
  static void setVideoReceiver(const Nan::FunctionCallbackInfo<v8::Value>& info);
  
  static Nan::Persistent<v8::Function> constructor;
};

#endif
