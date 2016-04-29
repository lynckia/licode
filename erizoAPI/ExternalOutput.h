#ifndef EXTERNALOUTPUT_H
#define EXTERNALOUTPUT_H

#include <nan.h>
#include <media/ExternalOutput.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"


/*
 * Wrapper class of erizo::ExternalOutput
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class ExternalOutput: public MediaSink {
 public:
  static void Init(v8::Local<v8::Object> exports);
  erizo::ExternalOutput* me;

 private:
  ExternalOutput();
  ~ExternalOutput();

  /*
   * Constructor.
   * Constructs a ExternalOutput
   */
  static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Closes the ExternalOutput.
   * The object cannot be used after this call
   */
  static void close(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Inits the ExternalOutput 
   * Returns true ready
   */
  static void init(const Nan::FunctionCallbackInfo<v8::Value>& info);
  
  static Nan::Persistent<v8::Function> constructor;
};

#endif
