#ifndef API_ONETOMANYPROCESSOR_H
#define API_ONETOMANYPROCESSOR_H

#include <nan.h>
#include <OneToManyProcessor.h>
#include <WebRtcConnection.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"
#include "ExternalInput.h"
#include "ExternalOutput.h"


/*
 * Wrapper class of erizo::OneToManyProcessor
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class OneToManyProcessor : public MediaSink {
 public:
  static void Init(v8::Local<v8::Object> exports);
  erizo::OneToManyProcessor* me;

 private:
  OneToManyProcessor();
  ~OneToManyProcessor();

  /*
   * Constructor.
   * Constructs a OneToManyProcessor
   */
  static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Closes the OneToManyProcessor.
   * The object cannot be used after this call
   */
  static void close(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Sets the Publisher
   * Param: the WebRtcConnection of the Publisher
   */
  static void setPublisher(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Adds an ExternalOutput
   * Param: The ExternalOutput   
   */
  static void addExternalOutput(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Sets an External Publisher
   * Param: the ExternalInput of the Publisher
   */
  static void setExternalPublisher(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Gets the Publisher state
   * Param: none
   */
  static void getPublisherState(const Nan::FunctionCallbackInfo<v8::Value>& info);
   /*
   * Returns true if OneToManyProcessor has a publisher
   */
  static void hasPublisher(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Sets the subscriber
   * Param1: the WebRtcConnection of the subscriber
   * Param2: an unique Id for the subscriber
   */
  static void addSubscriber(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Removes a subscriber given its peer id
   * Param: the peerId
   */
  static void removeSubscriber(const Nan::FunctionCallbackInfo<v8::Value>& info);
  
  static Nan::Persistent<v8::Function> constructor;
};

#endif
