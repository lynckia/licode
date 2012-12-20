#ifndef EXTERNALINPUT_H
#define EXTERNALINPUT_H

#include <node.h>
#include <media/ExternalInput.h>
#include "MediaReceiver.h"
#include "WebRtcConnection.h"


/*
 * Wrapper class of erizo::ExternalInput
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class ExternalInput : public MediaReceiver {
 public:
  static void Init(v8::Handle<v8::Object> target);

 private:
  ExternalInput();
  ~ExternalInput();

  /*
   * Constructor.
   * Constructs a ExternalInput
   */
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  /*
   * Closes the ExternalInput.
   * The object cannot be used after this call
   */
  static v8::Handle<v8::Value> close(const v8::Arguments& args);
  /*
   * Sets the Publisher
   * Param: the WebRtcConnection of the Publisher
   */
  static v8::Handle<v8::Value> setPublisher(const v8::Arguments& args);
   /*
   * Returns true if ExternalInput has a publisher
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
