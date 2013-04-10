#ifndef ONETOMANYPROCESSOR_H
#define ONETOMANYPROCESSOR_H

#include <node.h>
#include <OneToManyProcessor.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"
#include "ExternalInput.h"


/*
 * Wrapper class of erizo::OneToManyProcessor
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class OneToManyProcessor : public MediaSink {
 public:
  static void Init(v8::Handle<v8::Object> target);
  erizo::OneToManyProcessor* me;

 private:
  OneToManyProcessor();
  ~OneToManyProcessor();

  /*
   * Constructor.
   * Constructs a OneToManyProcessor
   */
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  /*
   * Closes the OneToManyProcessor.
   * The object cannot be used after this call
   */
  static v8::Handle<v8::Value> close(const v8::Arguments& args);
  /*
   * Sets the Publisher
   * Param: the WebRtcConnection of the Publisher
   */
  static v8::Handle<v8::Value> setPublisher(const v8::Arguments& args);
  /*
   * Sets an External Publisher
   * Param: the ExternalInput of the Publisher
   */
  static v8::Handle<v8::Value> setExternalPublisher(const v8::Arguments& args);
  /*
   * Returns true if OneToManyProcessor has a publisher
   */
  static v8::Handle<v8::Value> hasPublisher(const v8::Arguments& args);
  /*
   * Sets the subscriber
   * Param1: the WebRtcConnection of the subscriber
   * Param2: an unique Id for the subscriber
   */
  static v8::Handle<v8::Value> addSubscriber(const v8::Arguments& args);
  /*
   * Removes a subscriber given its peer id
   * Param: the peerId
   */
  static v8::Handle<v8::Value> removeSubscriber(const v8::Arguments& args);
  /*
   * Ask the publisher to send a FIR packet
   */
  static v8::Handle<v8::Value> sendFIR(const v8::Arguments& args);
};

#endif
