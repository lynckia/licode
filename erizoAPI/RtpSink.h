#ifndef RTPSINK_H
#define RTPSINK_H

#include <node.h>
#include <rtp/RtpSink.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"


/*
 * Wrapper class of erizo::RtpSink
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class RtpSink: public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> target);
  erizo::RtpSink* me;

 private:
  RtpSink();
  ~RtpSink();

  /*
   * Constructor.
   * Constructs a RtpSink
   */
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  /*
   * Closes the RtpSink.
   * The object cannot be used after this call
   */
  static v8::Handle<v8::Value> close(const v8::Arguments& args);
  /*
   * Inits the RtpSink 
   * Returns true ready
   */
  static v8::Handle<v8::Value> init(const v8::Arguments& args);  
  
  static v8::Handle<v8::Value> getFeedbackPort(const v8::Arguments& args);
};

#endif
