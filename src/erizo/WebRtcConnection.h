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

enum IceState {
	INITIAL, CANDIDATES_GATHERED, CANDIDATES_RECEIVED, READY, FINISHED, FAILED
};

class WebRtcConnectionStateListener {
public:
	virtual ~WebRtcConnectionStateListener() {
	};
	virtual void connectionStateChanged(IceState newState)=0;

};

/**
 * A WebRTC Connection. This class represents a WebRTC Connection that can be established with other peers via a SDP negotiation
 * it comprises all the necessary ICE and SRTP components.
 */
class WebRtcConnection: public MediaReceiver, public NiceReceiver {
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
	 * This is a blocking call and will end when candidates are gathered and the WebRTCConnection is ready to receive the remote SDP
	 * @return True if the candidates are gathered.
	 */
	bool init();
	/**
	 * Closes the webRTC connection.
	 * The object cannot be used after this call.
	 */
	void close();
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

	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);
	/**
	 * Sets a MediaReceiver that is going to receive Audio Data
	 * @param receiv The MediaReceiver to send audio to.
	 */
	void setAudioReceiver(MediaReceiver *receiv);
	/**
	 * Sets a MediaReceiver that is going to receive Video Data
	 * @param receiv The MediaReceiver
	 */
	void setVideoReceiver(MediaReceiver *receiv);
	/**
	 * Method to Receive data from a NiceConnection
	 * @param buf The data buffer
	 * @param len The length of the buffer
	 * @param nice The NiceConnection orgi
	 * @return
	 */

	int receiveNiceData(char* buf, int len, NiceConnection *nice);

	/**
	 * Sends a FIR Packet (RFC 5104) asking for a keyframe
	 * @return the size of the data sent
	 */
	int sendFirPacket();

	void setWebRTCConnectionStateListener(WebRtcConnectionStateListener* listener);

	IceState getCurrentState();

private:
	SdpInfo remoteSdp_;
	SdpInfo localSdp_;
	NiceConnection* audioNice_;
	NiceConnection* videoNice_;
	SrtpChannel* audioSrtp_;
	SrtpChannel* videoSrtp_;
	IceState globalIceState_;

	MediaReceiver* audioReceiver_;
	MediaReceiver* videoReceiver_;
	int video_, audio_, bundle_, sequenceNumberFIR_;
	unsigned int localAudioSsrc_, localVideoSsrc_;
	unsigned int remoteAudioSSRC_, remoteVideoSSRC_;
	boost::mutex writeMutex_, receiveAudioMutex_, receiveVideoMutex_;
	boost::thread send_Thread_;
	std::queue<packet> sendQueue_;
	WebRtcConnectionStateListener* connStateListener_;

	void updateState(IceState newState, NiceConnection* niceConn);

	bool sending;
	void sendLoop();

	friend class NiceConnection;

};

} /* namespace erizo */
#endif /* WEBRTCCONNECTION_H_ */
