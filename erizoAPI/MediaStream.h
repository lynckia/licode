
#ifndef ERIZOAPI_MEDIASTREAM_H_
#define ERIZOAPI_MEDIASTREAM_H_

#include <nan.h>
#include <MediaStream.h>
#include <logger.h>
#include "FuturesManager.h"
#include "MediaDefinitions.h"
#include "OneToManyProcessor.h"

#include <queue>
#include <string>
#include <future>  // NOLINT

class StatCallWorker : public Nan::AsyncWorker {
 public:
  StatCallWorker(Nan::Callback *callback, std::weak_ptr<erizo::MediaStream> weak_stream);

  void Execute();

  void HandleOKCallback();

 private:
  std::weak_ptr<erizo::MediaStream> weak_stream_;
  std::string stat_;
};

/*
 * Wrapper class of erizo::MediaStream
 *
 * A WebRTC Connection. This class represents a MediaStream that can be established with other peers via a SDP negotiation
 * it comprises all the necessary ICE and SRTP components.
 */
class MediaStream : public MediaSink, public erizo::MediaStreamStatsListener, public erizo::MediaStreamEventListener {
 public:
    DECLARE_LOGGER();
    static NAN_MODULE_INIT(Init);

    std::shared_ptr<erizo::MediaStream> me;
    std::queue<std::string> stats_messages;
    std::queue<std::pair<std::string, std::string>> event_messages;
    std::queue<Nan::Persistent<v8::Promise::Resolver> *> futures;
    FuturesManager futures_manager_;

    boost::mutex mutex;

 private:
    MediaStream();
    ~MediaStream();

    boost::future<void> close();
    void closeEvents();
    std::string toLog();

    Nan::Callback *event_callback_;
    uv_async_t *async_event_;
    bool has_event_callback_;

    Nan::Callback *stats_callback_;
    uv_async_t *async_stats_;

    uv_async_t *close_future_async_;
    bool has_stats_callback_;
    bool closed_;
    std::string id_;
    std::string label_;
    /*
     * Constructor.
     * Constructs an empty MediaStream without any configuration.
     */
    static NAN_METHOD(New);
    /*
     * Closes the MediaStream.
     * The object cannot be used after this call.
     */
    static NAN_METHOD(close);
    /*
     * Configures the MediaStream.
     */
    static NAN_METHOD(configure);
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
     * Request a PLI packet from this MediaStream
     */
    static NAN_METHOD(generatePLIPacket);
    /*
     * Enables or disables Feedback reports from this MediaStream
     * Param: A boolean indicating what to do
     */
    static NAN_METHOD(setFeedbackReports);
    /*
     * Enables or disables SlideShowMode for this MediaStream
     * Param: A boolean indicating what to do
     */
    static NAN_METHOD(setSlideShowMode);
    /*
     * Mutes or unmutes streams for this MediaStream
     * Param: A boolean indicating what to do
     */
    static NAN_METHOD(muteStream);
    /*
     * Sets Max Video BW
     * Param: The value for the max video bandwidth
     */
    static NAN_METHOD(setMaxVideoBW);
    /*
     * Sets constraints to the subscribing video
     * Param: Max width, height and framerate.
     */
    static NAN_METHOD(setVideoConstraints);
    /*
     * Gets Stats from this MediaStream
     * Param: None
     * Returns: The Current stats
     * Param: Callback that will get periodic stats reports
     * Returns: True if the callback was set successfully
     */
    static NAN_METHOD(getStats);

    /*
     * Gets Stats from this MediaStream
     * Param: None
     * Returns: The Current stats
     * Param: Callback that will get periodic stats reports
     * Returns: True if the callback was set successfully
     */
    static NAN_METHOD(getPeriodicStats);

    /*
     * Sets Metadata that will be logged in every message
     * Param: An object with metadata {key1:value1, key2: value2}
     */
    static NAN_METHOD(setMetadata);
    static NAN_METHOD(setPeriodicKeyframeRequests);
    static NAN_METHOD(hasPeriodicKeyframeRequests);
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
    static NAN_METHOD(enableSlideShowBelowSpatialLayer);

    static NAN_METHOD(onMediaStreamEvent);

    static Nan::Persistent<v8::Function> constructor;

    static NAUV_WORK_CB(statsCallback);
    virtual void notifyStats(const std::string& message);

    static NAUV_WORK_CB(eventCallback);
    virtual void notifyMediaStreamEvent(const std::string& type = "",
        const std::string& message = "");
    static NAUV_WORK_CB(closePromiseResolver);
    virtual void notifyFuture(Nan::Persistent<v8::Promise::Resolver> *persistent);
};

#endif  // ERIZOAPI_MEDIASTREAM_H_
