/*
 * WebRTCConnection.cpp
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#include "WebRTCConnection.h"

WebRTCConnection::WebRTCConnection(): MediaReceiver() {
	// TODO Auto-generated constructor stub

}

WebRTCConnection::~WebRTCConnection() {
	// TODO Auto-generated destructor stub
}

bool WebRTCConnection::init(){
	return true;
}
bool WebRTCConnection::setRemoteSDP(const std::string &sdp){
	return true;
}
const char* WebRTCConnection::getLocalSDP(){
	return "";
}
void WebRTCConnection::setAudioReceiver(MediaReceiver *receiv){
	this->audio_receiver = receiv;
}
void WebRTCConnection::setVideoReceiver(MediaReceiver *receiv){
	this->video_receiver = receiv;
}

int WebRTCConnection::receiveAudioData(char* buf, int len){
	int res=-1;
	int length= len;
	if (audio_srtp){
		length = audio_srtp->ProtectRtp(buf, len);
	}
	if (audio_nice){
		res= audio_nice->sendData(buf, length);
	}
	return res;
}
int WebRTCConnection::receiveVideoData(char* buf, int len){
	int res=-1;
	int length= len;
	if (video_srtp){
		length = video_srtp->ProtectRtp(buf, len);
	}
	if (video_nice){
		res= video_nice->sendData(buf, length);
	}
	return res;
}

int WebRTCConnection::receiveNiceData(char* buf, int len, NiceConnection* nice){
	int length = len;
	if(nice == audio_nice){
		if (audio_receiver){
			if (audio_srtp){
				length = audio_srtp->UnprotectRtp(buf,len);
			}
			audio_receiver->receiveAudioData(buf, length);
			return length;
		}
	}
	else if(nice == video_nice){
		if (video_receiver){
			if (video_srtp){
				length = video_srtp->UnprotectRtp(buf,len);
			}
			video_receiver->receiveVideoData(buf, length);
		}
	}
	return -1;
}


