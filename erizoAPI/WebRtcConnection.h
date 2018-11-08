#ifndef ERIZOAPI_WEBRTCCONNECTION_H_
#define ERIZOAPI_WEBRTCCONNECTION_H_

#include <nan.h>
#include <WebRtcConnection.h>
#include <logger.h>
#include "MediaDefinitions.h"
#include "OneToManyProcessor.h"
#include "ConnectionDescription.h"

#include <queue>
#include <string>
#include <future>  // NOLINT

/*
 * Wrapper class of erizo::WebRtcConnection
 *
 * A WebRTC Connection. This class represents a WebRtcConnection that can be established with other peers via a SDP negotiation
 * it comprises all the necessary ICE and SRTP components.
 */
class WebRtcConnection : public erizo::WebRtcConnectionEventListener,
   public Nan::ObjectWrap{
 public:
    DECLARE_LOGGER();
    static NAN_MODULE_INIT(Init);

    std::shared_ptr<erizo::WebRtcConnection> me;
    std::queue<int> event_status;
    std::queue<std::pair<std::string, std::string>> event_messages;

    boost::mutex mutex;

 private:
    WebRtcConnection();
    ~WebRtcConnection();

    std::string toLog();
    void close();

    Nan::Callback *event_callback_;
    uv_async_t *async_;
    bool closed_;
    std::string id_;
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
    static NAN_METHOD(setRemoteDescription);
    /*
     * Gets the SDP of the local peer.
     * Param: the SDP.
     * Returns true if the SDP was received correctly.
     */
    static NAN_METHOD(getLocalDescription);
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
     * Gets the current state of the Ice Connection
     * Returns the state.
     */
    static NAN_METHOD(getCurrentState);
    /*
     * Sets Metadata that will be logged in every message
     * Param: An object with metadata {key1:value1, key2: value2}
     */
    static NAN_METHOD(setMetadata);

    static NAN_METHOD(addMediaStream);
    static NAN_METHOD(removeMediaStream);

    static Nan::Persistent<v8::Function> constructor;

    static NAUV_WORK_CB(eventsCallback);

    virtual void notifyEvent(erizo::WebRTCEvent event,
                             const std::string& message = "",
                             const std::string& stream_id = "");
};

#endif  // ERIZOAPI_WEBRTCCONNECTION_H_
