#ifndef ERIZOAPI_WEBRTCCONNECTION_H_
#define ERIZOAPI_WEBRTCCONNECTION_H_

#include <nan.h>
#include <WebRtcConnection.h>
#include "MediaDefinitions.h"
#include "OneToManyProcessor.h"

#include <queue>
#include <string>
#include <future>  // NOLINT

class StatCallWorker : public Nan::AsyncWorker {
 public:
  StatCallWorker(Nan::Callback *callback, std::weak_ptr<erizo::WebRtcConnection> weak_connection);

  void Execute();

  void HandleOKCallback();

 private:
  std::weak_ptr<erizo::WebRtcConnection> weak_connection_;
  std::string stat_;
};

/*
 * Wrapper class of erizo::WebRtcConnection
 *
 * A WebRTC Connection. This class represents a WebRtcConnection that can be established with other peers via a SDP negotiation
 * it comprises all the necessary ICE and SRTP components.
 */
class WebRtcConnection : public MediaSink, public erizo::WebRtcConnectionEventListener,
  public erizo::WebRtcConnectionStatsListener {
 public:
    static NAN_MODULE_INIT(Init);

    std::shared_ptr<erizo::WebRtcConnection> me;
    int eventSt;
    std::queue<int> eventSts;
    std::queue<std::string> eventMsgs, statsMsgs;

    boost::mutex mutex;

 private:
    WebRtcConnection();
    ~WebRtcConnection();

    Nan::Callback *eventCallback_;
    Nan::Callback *statsCallback_;

    uv_async_t async_;
    uv_async_t asyncStats_;
    bool hasCallback_;
    /*
     * Constructor.
     * Constructs an empty WebRtcConnection without any configuration.
     */
    static NAN_METHOD(New);
    /*
     * Closes the webRTC connection.
     * The object cannot be used after this call.
     */
    static NAN_METHOD(close);
    /*
     * Inits the WebRtcConnection and passes the callback to get Events.
     * Returns true if the candidates are gathered.
     */
    static NAN_METHOD(init);
    /*
     * Creates an SDP Offer
     * Param: No params.
     * Returns true if the process has started successfully.
     */
    static NAN_METHOD(createOffer);
    /*
     * Sets the SDP of the remote peer.
     * Param: the SDP.
     * Returns true if the SDP was received correctly.
     */
    static NAN_METHOD(setRemoteSdp);
    /**
     * Add new remote candidate (from remote peer).
     * @param sdp The candidate in SDP format.
     * @return true if the SDP was received correctly.
     */
    static NAN_METHOD(addRemoteCandidate);
    /*
     * Obtains the local SDP.
     * Returns the SDP as a string.
     */
    static NAN_METHOD(getLocalSdp);
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
     * Gets Stats from this Wrtc
     * Param: None
     * Returns: The Current stats
     * Param: Callback that will get periodic stats reports
     * Returns: True if the callback was set successfully
     */
    static NAN_METHOD(getStats);

    /*
     * Gets Stats from this Wrtc
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

    static NAUV_WORK_CB(eventsCallback);
    static NAUV_WORK_CB(statsCallback);

    virtual void notifyEvent(erizo::WebRTCEvent event, const std::string& message = "");
    virtual void notifyStats(const std::string& message);
};

#endif  // ERIZOAPI_WEBRTCCONNECTION_H_
