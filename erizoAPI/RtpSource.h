#ifndef RTPSOURCE_H
#define RTPSOURCE_H

#include <node.h>
#include <rtp/RtpSource.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"


/*
 * Wrapper class of erizo::RtpSource
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class RtpSource: public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> target);
  erizo::RtpSource* me;

 private:
  RtpSource();
  ~RtpSource();

  /*
   * Constructor.
   * Constructs a RtpSource
   */
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  /*
   * Closes the RtpSource.
   * The object cannot be used after this call
   */
  static v8::Handle<v8::Value> close(const v8::Arguments& args);
  /*
   * Inits the RtpSource 
   * Returns true ready
   */
  static v8::Handle<v8::Value> init(const v8::Arguments& args);  
  /*
   * Sets a MediaReceiver that is going to receive Audio Data
   * Param: the MediaReceiver to send audio to.
   */
  static v8::Handle<v8::Value> setAudioReceiver(const v8::Arguments& args);
  /*
   * Sets a MediaReceiver that is going to receive Video Data
   * Param: the MediaReceiver
   */
  static v8::Handle<v8::Value> setVideoReceiver(const v8::Arguments& args);
};

#endif
