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
    static NAN_MODULE_INIT(Init);
    erizo::ExternalOutput* me;

  private:
    ExternalOutput();
    ~ExternalOutput();

    /*
     * Constructor.
     * Constructs a ExternalOutput
     */
    static NAN_METHOD(New);
    /*
     * Closes the ExternalOutput.
     * The object cannot be used after this call
     */
    static NAN_METHOD(close);
    /*
     * Inits the ExternalOutput 
     * Returns true ready
     */
    static NAN_METHOD(init);

    static Nan::Persistent<v8::Function> constructor;
};

#endif
