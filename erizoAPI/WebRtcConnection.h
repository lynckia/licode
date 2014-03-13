#ifndef WEBRTCCONNECTION_H
#define WEBRTCCONNECTION_H

#include <node.h>
#include <WebRtcConnection.h>
#include "MediaDefinitions.h"
#include "OneToManyProcessor.h"


/*
 * Wrapper class of erizo::WebRtcConnection
 *
 * A WebRTC Connection. This class represents a WebRtcConnection that can be established with other peers via a SDP negotiation
 * it comprises all the necessary ICE and SRTP components.
 */
class WebRtcConnection : public node::ObjectWrap, erizo::WebRtcConnectionEventListener  {
 public:
  static void Init(v8::Handle<v8::Object> target);

  erizo::WebRtcConnection *me;
  int message;

 private:
  WebRtcConnection();
  ~WebRtcConnection();
  
  v8::Persistent<v8::Function> eventCallback;

  uv_async_t async;
  bool hasCallback_;
  /*
   * Constructor.
   * Constructs an empty WebRtcConnection without any configuration.
   */
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  /*
   * Closes the webRTC connection.
   * The object cannot be used after this call.
   */
  static v8::Handle<v8::Value> close(const v8::Arguments& args);
  /*
   * Inits the WebRtcConnection and passes the callback to get Events.
   * Returns true if the candidates are gathered.
   */
  static v8::Handle<v8::Value> init(const v8::Arguments& args);  
  /*
   * Sets the SDP of the remote peer.
   * Param: the SDP.
   * Returns true if the SDP was received correctly.
   */
  static v8::Handle<v8::Value> setRemoteSdp(const v8::Arguments& args);
  /*
   * Obtains the local SDP.
   * Returns the SDP as a string.
   */
  static v8::Handle<v8::Value> getLocalSdp(const v8::Arguments& args);
  /*
   * Sets a MediaReceiver that is going to receive Audio Data
   * Param: the MediaReceiver to send audio to.
   */
  static v8::Handle<v8::Value> setAudioReceiver(const v8::Arguments& args);
  /*
   * Sets a MediaReceiver that is going to receive Video Data
   * Param: the MediaReceiver
   */
  static v8::Handle<v8::Value> setVideoReceiver(const v8::Arguments& args);
  /*
   * Gets the current state of the Ice Connection
   * Returns the state.
   */
  static v8::Handle<v8::Value> getCurrentState(const v8::Arguments& args);
  
  static void after_cb(uv_async_t *handle, int status);
 
	virtual void notifyEvent(erizo::WebRTCEvent event, const std::string& message="");
};

#endif
