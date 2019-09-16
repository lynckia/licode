#ifndef ERIZOAPI_EXTERNALINPUT_H_
#define ERIZOAPI_EXTERNALINPUT_H_

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
class ExternalInput : public Nan::ObjectWrap {
 public:
    static NAN_MODULE_INIT(Init);
    std::shared_ptr<erizo::ExternalInput> me;

 private:
    ExternalInput();
    ~ExternalInput();

    /*
     * Constructor.
     * Constructs a ExternalInput
     */
    static NAN_METHOD(New);
    /*
     * Closes the ExternalInput.
     * The object cannot be used after this call
     */
    static NAN_METHOD(close);
    /*
     * Inits the ExternalInput
     * Returns true ready
     */
    static NAN_METHOD(init);
    /*
     * Sets a MediaSink that is going to receive Audio Data
     * Param: the MediaSink to send audio to.
     */
    static NAN_METHOD(setAudioReceiver);
    /*
     * Sets a MediaSink that is going to receive Video Data
     * Param: the MediaSink
     */
    static NAN_METHOD(setVideoReceiver);
    /*
     * Request a PLI packet from this ExternalInput
     */
    static NAN_METHOD(generatePLIPacket);

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_EXTERNALINPUT_H_
