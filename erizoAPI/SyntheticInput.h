#ifndef ERIZOAPI_SYNTHETICINPUT_H_
#define ERIZOAPI_SYNTHETICINPUT_H_

#include <nan.h>
#include <media/SyntheticInput.h>
#include "MediaDefinitions.h"
#include "MediaStream.h"


/*
 * Wrapper class of erizo::SyntheticInput
 *
 * Represents a SyntheticInput connection.
 */
class SyntheticInput : public Nan::ObjectWrap {
 public:
    static NAN_MODULE_INIT(Init);
    std::shared_ptr<erizo::SyntheticInput> me;

 private:
    SyntheticInput();
    ~SyntheticInput();

    /*
     * Constructor.
     * Constructs a SyntheticInput
     */
    static NAN_METHOD(New);
    /*
     * Closes the SyntheticInput.
     * The object cannot be used after this call
     */
    static NAN_METHOD(close);
    /*
     * Inits the SyntheticInput
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

    static NAN_METHOD(setFeedbackSource);

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_SYNTHETICINPUT_H_
