#ifndef EXTERNALINPUT_H
#define EXTERNALINPUT_H

#include <node.h>
#include <media/ExternalInput.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"


/*
 * Wrapper class of erizo::ExternalInput
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class ExternalInput : public MediaSource {
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
   * Inits the ExternalInput 
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
