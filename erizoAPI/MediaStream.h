
#ifndef ERIZOAPI_MEDIASTREAM_H_
#define ERIZOAPI_MEDIASTREAM_H_

#include <nan.h>
#include <MediaStream.h>
#include "MediaDefinitions.h"
#include "OneToManyProcessor.h"

#include <queue>
#include <string>
#include <future>  // NOLINT

/*
 * Wrapper class of erizo::MediaStream
 *
 * A WebRTC Connection. This class represents a MediaStream that can be established with other peers via a SDP negotiation
 * it comprises all the necessary ICE and SRTP components.
 */
class MediaStream : public MediaSink {
 public:
    static NAN_MODULE_INIT(Init);

    std::shared_ptr<erizo::MediaStream> me;

 private:
    MediaStream();
    ~MediaStream();

    /*
     * Constructor.
     * Constructs an empty MediaStream without any configuration.
     */
    static NAN_METHOD(New);
    /*
     * Closes the webRTC connection.
     * The object cannot be used after this call.
     */
    static NAN_METHOD(close);
    /*
     * Inits the MediaStream and passes the callback to get Events.
     * Returns true if the candidates are gathered.
     */
    static NAN_METHOD(init);
    /*
     * Sets a MediaReceiver that is going to receive Audio Data
     * Param: the MediaReceiver to send audio to.
     */
    static NAN_METHOD(setAudioReceiver);
    /*
     * Sets a MediaReceiver that is going to receive Video Data
     * Param: the MediaReceiver
     */
    static NAN_METHOD(setVideoReceiver);
    /*
     * Gets the current state of the Ice Connection
     * Returns the state.
     */
    static NAN_METHOD(getCurrentState);
    /*
     * Request a PLI packet from this WRTCConn
     */
    static NAN_METHOD(generatePLIPacket);
    /*
     * Enables or disables Feedback reports from this WRTC
     * Param: A boolean indicating what to do
     */
    static NAN_METHOD(setFeedbackReports);
    /*
     * Enables or disables SlideShowMode for this WRTC
     * Param: A boolean indicating what to do
     */
    static NAN_METHOD(setSlideShowMode);
    /*
     * Mutes or unmutes streams for this WRTC
     * Param: A boolean indicating what to do
     */
    static NAN_METHOD(muteStream);
    /*
     * Sets constraints to the subscribing video
     * Param: Max width, height and framerate.
     */
    static NAN_METHOD(setVideoConstraints);

    /*
     * Sets Metadata that will be logged in every message
     * Param: An object with metadata {key1:value1, key2: value2}
     */
    static NAN_METHOD(setMetadata);
    /*
     * Enable a specific Handler in the pipeline
     * Param: Name of the handler
     */
    static NAN_METHOD(enableHandler);
    /*
     * Disables a specific Handler in the pipeline
     * Param: Name of the handler
     */
    static NAN_METHOD(disableHandler);

    static NAN_METHOD(setQualityLayer);

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_MEDIASTREAM_H_
