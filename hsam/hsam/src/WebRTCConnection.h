/*
 * WebRTCConnection.h
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#ifndef WEBRTCCONNECTION_H_
#define WEBRTCCONNECTION_H_

#include "NiceConnection.h"
#include "Srtpchannel.h"
#include "sdpinfo.h"

class WebRTCConnection {
public:
	WebRTCConnection();
	virtual ~WebRTCConnection();
	bool init();
	bool setRemoteSDP(const char* sdp);
	const char* getLocalSDP();
private:
	SDPInfo remote_sdp;
	SDPInfo local_sdp;
	NiceConnection *nice;

};

#endif /* WEBRTCCONNECTION_H_ */
