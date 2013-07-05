#ifndef WEBRTCCONNECTION_H_
#define WEBRTCCONNECTION_H_

#include <string>
#include <queue>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include "SrtpChannel.h"
#include "SdpInfo.h"
#include "MediaDefinitions.h"

namespace erizo {

class NiceConnection;
class DtlsConnection;
/**
 * States of ICE
 */
enum IceState {
	INITIAL, CANDIDATES_GATHERED, CANDIDATES_RECEIVED, READY, FINISHED, FAILED
};

class WebRtcConnectionStateListener {
public:
	virtual ~WebRtcConnectionStateListener() {
	}
	;
	virtual void connectionStateChanged(IceState newState)=0;

};

/**
 * A WebRTC Connection. This class represents a WebRTC Connection that can be established with other peers via a SDP negotiation
 * it comprises all the necessary ICE and SRTP components.
 */
class WebRtcConnection: public MediaSink, public MediaSource, public NiceReceiver, public FeedbackSink, public FeedbackSource {
public:
	/**
	 * Constructor.
	 * Constructs an empty WebRTCConnection without any configuration.
	 */
	WebRtcConnection();
	/**
	 * Destructor.
	 */
	virtual ~WebRtcConnection();
	/**
	 * Inits the WebConnection by starting ICE Candidate Gathering.
	 * @return True if the candidates are gathered.
	 */
	bool init();
	/**
	 * Closes the webRTC connection.
	 * The object cannot be used after this call.
	 */
	void close();
  void closeSink();
  void closeSource();
	/**
	 * Sets the SDP of the remote peer.
	 * @param sdp The SDP.
	 * @return true if the SDP was received correctly.
	 */
	bool setRemoteSdp(const std::string &sdp);
	/**
	 * Obtains the local SDP.
	 * @return The SDP as a string.
	 */
	std::string getLocalSdp();

	int deliverAudioData(char* buf, int len);
	int deliverVideoData(char* buf, int len);

	/**
	 * Method to Receive data from a NiceConnection
	 * @param buf The data buffer
	 * @param len The length of the buffer
	 * @param nice The NiceConnection orgi
	 * @return
	 */
	int receiveNiceData(char* buf, int len, NiceConnection *nice);


  int deliverFeedback(char* buf, int len);

	/**
	 * Sends a FIR Packet (RFC 5104) asking for a keyframe
	 * @return the size of the data sent
	 */
	int sendFirPacket();

	void setWebRTCConnectionStateListener(
			WebRtcConnectionStateListener* listener);
	/**
	 * Gets the current state of the Ice Connection
	 * @return
	 */
	IceState getCurrentState();

	void startSRTP(std::string clientKey,std::string serverKey, std::string srtpProfile);

private:
	SdpInfo remoteSdp_;
	SdpInfo localSdp_;
	NiceConnection* videoNice_;
	SrtpChannel* audioSrtp_;
	SrtpChannel* videoSrtp_;
	IceState globalIceState_;

	int video_, audio_, bundle_, sequenceNumberFIR_;
	boost::mutex writeMutex_, receiveAudioMutex_, receiveVideoMutex_, updateStateMutex_;
	boost::thread send_Thread_;
	std::queue<dataPacket> sendQueue_;
	WebRtcConnectionStateListener* connStateListener_;

	DtlsConnection* audioDtls_;
	DtlsConnection* videoDtls_;

	void updateState(IceState newState, NiceConnection* niceConn);

	bool sending;
	void sendLoop();

	friend class NiceConnection;

};

} /* namespace erizo */
#endif /* WEBRTCCONNECTION_H_ */
