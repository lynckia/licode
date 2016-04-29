#ifndef API_WEBRTCCONNECTION_H
#define API_WEBRTCCONNECTION_H

#include <nan.h>
#include <WebRtcConnection.h>
#include "MediaDefinitions.h"
#include "OneToManyProcessor.h"


/*
 * Wrapper class of erizo::WebRtcConnection
 *
 * A WebRTC Connection. This class represents a WebRtcConnection that can be established with other peers via a SDP negotiation
 * it comprises all the necessary ICE and SRTP components.
 */
class WebRtcConnection : public MediaSink, erizo::WebRtcConnectionEventListener, erizo::WebRtcConnectionStatsListener  {
 public:
  static void Init(v8::Local<v8::Object> exports);

  erizo::WebRtcConnection *me;
  int eventSt;
  std::queue<int> eventSts;
  std::queue<std::string> eventMsgs, statsMsgs;

  boost::mutex mutex;

 private:
  WebRtcConnection();
  ~WebRtcConnection();
  
  v8::Persistent<v8::Function> eventCallback_;
  v8::Persistent<v8::Function> statsCallback_;

  uv_async_t async_;
  uv_async_t asyncStats_;
  bool hasCallback_;
  /*
   * Constructor.
   * Constructs an empty WebRtcConnection without any configuration.
   */
  static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Closes the webRTC connection.
   * The object cannot be used after this call.
   */
  static void close(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Inits the WebRtcConnection and passes the callback to get Events.
   * Returns true if the candidates are gathered.
   */
  static void init(const Nan::FunctionCallbackInfo<v8::Value>& info);  
  /*
   * Sets the SDP of the remote peer.
   * Param: the SDP.
   * Returns true if the SDP was received correctly.
   */
  static void createOffer(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void setRemoteSdp(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /**
     * Add new remote candidate (from remote peer).
     * @param sdp The candidate in SDP format.
     * @return true if the SDP was received correctly.
     */
  static void  addRemoteCandidate(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Obtains the local SDP.
   * Returns the SDP as a string.
   */
  static void getLocalSdp(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Sets a MediaReceiver that is going to receive Audio Data
   * Param: the MediaReceiver to send audio to.
   */
  static void setAudioReceiver(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Sets a MediaReceiver that is going to receive Video Data
   * Param: the MediaReceiver
   */
  static void setVideoReceiver(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Gets the current state of the Ice Connection
   * Returns the state.
   */
  static void getCurrentState(const Nan::FunctionCallbackInfo<v8::Value>& info);
  /*
   * Request a PLI packet from this WRTCConn
   */
  static void generatePLIPacket(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void setFeedbackReports(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void setSlideShowMode(const Nan::FunctionCallbackInfo<v8::Value>& info);

  static void getStats(const Nan::FunctionCallbackInfo<v8::Value>& info);  
  
  static Nan::Persistent<v8::Function> constructor;

  static void eventsCallback(uv_async_t *handle, int status);
  static void statsCallback(uv_async_t *handle, int status);
 
	virtual void notifyEvent(erizo::WebRTCEvent event, const std::string& message="");
	virtual void notifyStats(const std::string& message);
};

#endif
