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
#include "mediadefinitions.h"
#include <string>
#include <boost/thread/mutex.hpp>

class WebRTCConnection: public MediaReceiver, public NiceReceiver{
public:
	WebRTCConnection(bool standAlone=false);
	virtual ~WebRTCConnection();
	bool init();
	void close();
	bool setRemoteSDP(const std::string &sdp);
	std::string getLocalSDP();
	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);
	void setAudioReceiver(MediaReceiver *receiv);
	void setVideoReceiver(MediaReceiver *receiv);
	int receiveNiceData(char* buf,int len, NiceConnection *nice);


private:
	SDPInfo remote_sdp;
	SDPInfo local_sdp;
	NiceConnection *audio_nice;
	NiceConnection *video_nice;
	NiceConnection *audio_nice_rtcp;
	NiceConnection *video_nice_rtcp;
	SrtpChannel *audio_srtp;
	SrtpChannel *video_srtp;

	MediaReceiver *audio_receiver;
	MediaReceiver *video_receiver;
	int video, audio;
	int audio_ssrc, video_ssrc;
	bool standAlone;

	//std::string getLocalAddress();
	boost::mutex write_mutex, receiveAudio, receiveVideo;

};

#endif /* WEBRTCCONNECTION_H_ */
