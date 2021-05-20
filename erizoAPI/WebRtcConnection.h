#ifndef ERIZOAPI_WEBRTCCONNECTION_H_
#define ERIZOAPI_WEBRTCCONNECTION_H_

#include <nan.h>
#include <WebRtcConnection.h>
#include <lib/Clock.h>
#include <logger.h>
#include <boost/variant.hpp>
#include "PromiseDurationDistribution.h"
#include "MediaDefinitions.h"
#include "OneToManyProcessor.h"
#include "ConnectionDescription.h"

#include <queue>
#include <string>
#include <future>  // NOLINT

typedef boost::variant<std::string, std::shared_ptr<erizo::SdpInfo>> ResultVariant;
typedef std::tuple<Nan::Persistent<v8::Promise::Resolver> *, ResultVariant, erizo::time_point, erizo::time_point>
   ResultTuple;

using erizo::duration;

class ConnectionStatCallWorker : public Nan::AsyncWorker {
 public:
  ConnectionStatCallWorker(Nan::Callback *callback, std::weak_ptr<erizo::WebRtcConnection> weak_connection);

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
class WebRtcConnection : public erizo::WebRtcConnectionEventListener,
   public Nan::ObjectWrap{
 public:
    DECLARE_LOGGER();
    static NAN_MODULE_INIT(Init);

    std::shared_ptr<erizo::WebRtcConnection> me;
    std::queue<int> event_status;
    std::queue<std::string> event_messages;
    std::queue<ResultTuple> futures;

    boost::mutex mutex;

    PromiseDurationDistribution promise_durations_;
    PromiseDurationDistribution promise_delays_;

 private:
    WebRtcConnection();
    ~WebRtcConnection();

    std::string toLog();
    void closeEvents();
    boost::future<std::string> close();
    void computePromiseTimes(erizo::time_point scheduled_at, erizo::time_point started, erizo::time_point end);
    erizo::BwDistributionConfig parseDistribConfig(std::string distribution_config_string);
    Nan::Callback *event_callback_;
    uv_async_t *async_;
    uv_async_t *future_async_;
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
    /**
     * Add new remote candidate (from remote peer).
     * @param sdp The candidate in SDP format.
     * @return true if the SDP was received correctly.
     */
    static NAN_METHOD(addRemoteCandidate);
    /*
     * Gets the current state of the Ice Connection
     * Returns the state.
     */
    static NAN_METHOD(getCurrentState);
    /*
     * Gets the current quality level of the WebRtcConnection
     * Returns the level.
     */
    static NAN_METHOD(getConnectionQualityLevel);
    /*
     * Sets Metadata that will be logged in every message
     * Param: An object with metadata {key1:value1, key2: value2}
     */
    static NAN_METHOD(setMetadata);

    static NAN_METHOD(addMediaStream);
    static NAN_METHOD(removeMediaStream);

    static NAN_METHOD(copySdpToLocalDescription);

    static NAN_METHOD(setBwDistributionConfig);

    static NAN_METHOD(getStats);

    static NAN_METHOD(maybeRestartIce);
    static NAN_METHOD(getDurationDistribution);
    static NAN_METHOD(getDelayDistribution);
    static NAN_METHOD(resetStats);

    static Nan::Persistent<v8::Function> constructor;

    static NAUV_WORK_CB(eventsCallback);
    static NAUV_WORK_CB(promiseResolver);

    virtual void notifyEvent(erizo::WebRTCEvent event,
                             const std::string& message = "");
    virtual void notifyFuture(Nan::Persistent<v8::Promise::Resolver> *persistent, erizo::time_point scheduled_at,
        ResultVariant result = ResultVariant());
};

#endif  // ERIZOAPI_WEBRTCCONNECTION_H_
