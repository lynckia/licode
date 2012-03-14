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

class WebRTCConnection {
public:
	WebRTCConnection();
	virtual ~WebRTCConnection();
	bool init();
	bool setRemoteSDP(const char* sdp);
	const char* getLocalSDP();
private:
	NiceConnection ice;
	SrtpChannel srtp;
};

#endif /* WEBRTCCONNECTION_H_ */
