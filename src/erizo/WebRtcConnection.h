#ifndef WEBRTCCONNECTION_H_
#define WEBRTCCONNECTION_H_

#include <string>
#include <boost/thread/mutex.hpp>

#include "SrtpChannel.h"
#include "SdpInfo.h"
#include "MediaDefinitions.h"

namespace erizo {

class NiceConnection;

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
	 * @param sdp The SDP
	 * @return true if the SDP was received correctly.
	 */
	bool setRemoteSdp(const std::string &sdp);
	/**
	 * Obtains the local SDP-
	 * @return The SDP as a string.
	 */
	std::string getLocalSdp();
	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);
	void setAudioReceiver(MediaReceiver *receiv);
	void setVideoReceiver(MediaReceiver *receiv);
	int receiveNiceData(char* buf, int len, NiceConnection *nice);

private:
	SdpInfo remoteSdp_;
	SdpInfo localSdp_;
	NiceConnection* audioNice_;
	NiceConnection* videoNice_;
	SrtpChannel* audioSrtp_;
	SrtpChannel* videoSrtp_;

	MediaReceiver* audioReceiver_;
	MediaReceiver* videoReceiver_;
	int video_, audio_, bundle_;
	unsigned int localAudioSsrc_, localVideoSsrc_;
	unsigned int remoteAudioSSRC_, remoteVideoSSRC_;
	boost::mutex writeMutex_, receiveAudioMutex_, receiveVideoMutex_;

};

} /* namespace erizo */
#endif /* WEBRTCCONNECTION_H_ */
