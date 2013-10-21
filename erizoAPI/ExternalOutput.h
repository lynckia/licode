#ifndef EXTERNALOUTPUT_H
#define EXTERNALOUTPUT_H

#include <node.h>
#include <media/ExternalOutput.h>
#include "MediaDefinitions.h"
#include "WebRtcConnection.h"


/*
 * Wrapper class of erizo::ExternalOutput
 *
 * Represents a OneToMany connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class ExternalOutput: public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> target);
  erizo::ExternalOutput* me;

 private:
  ExternalOutput();
  ~ExternalOutput();

  /*
   * Constructor.
   * Constructs a ExternalOutput
   */
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  /*
   * Closes the ExternalOutput.
   * The object cannot be used after this call
   */
  static v8::Handle<v8::Value> close(const v8::Arguments& args);
  /*
   * Inits the ExternalOutput 
   * Returns true ready
   */
  static v8::Handle<v8::Value> init(const v8::Arguments& args);  
};

#endif
