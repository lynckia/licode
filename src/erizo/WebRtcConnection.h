/*
 * WebRTCConnection.h
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#ifndef WEBRTCCONNECTION_H_
#define WEBRTCCONNECTION_H_

#include <string>
#include <boost/thread/mutex.hpp>

#include "SrtpChannel.h"
#include "SdpInfo.h"
#include "MediaDefinitions.h"

namespace erizo {

class NiceConnection;

class WebRtcConnection: public MediaReceiver, public NiceReceiver{
public:
	WebRtcConnection(bool standAlone=false);
	virtual ~WebRtcConnection();
	bool init();
	void close();
	bool setRemoteSdp(const std::string &sdp);
	std::string getLocalSdp();
	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);
	void setAudioReceiver(MediaReceiver *receiv);
	void setVideoReceiver(MediaReceiver *receiv);
	int receiveNiceData(char* buf,int len, NiceConnection *nice);


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
	unsigned int remoteAudioSSRC_,remoteVideoSSRC_;

	bool standAlone_;

	boost::mutex writeMutex_, receiveAudioMutex_, receiveVideoMutex_;

};

} /* namespace erizo */
#endif /* WEBRTCCONNECTION_H_ */
