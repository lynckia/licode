/*
 * WebRTCConnection.cpp
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#include "WebRTCConnection.h"
#include "sdpinfo.h"

#include <cstdio>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

WebRTCConnection::WebRTCConnection(): MediaReceiver() {
	// TODO Auto-generated constructor stub
	std::string address = getLocalAddress();
	printf("LocalAddress is %s\n", address.c_str());
	std::string *stun = new std::string("173.194.70.126");
	video_nice = new NiceConnection(address,*stun);

}

WebRTCConnection::~WebRTCConnection() {
	// TODO Auto-generated destructor stub
}

bool WebRTCConnection::init(){
	video_nice->init();

	return true;
}
bool WebRTCConnection::setRemoteSDP(const std::string &sdp){
	remote_sdp.initWithSDP(sdp);
	return true;
}
std::string WebRTCConnection::getLocalSDP(){
	return local_sdp.getSDP();
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
	if(nice->type == AUDIO_TYPE){
		if (audio_receiver){
			if (audio_srtp){
				length = audio_srtp->UnprotectRtp(buf,len);
			}
			audio_receiver->receiveAudioData(buf, length);
			return length;
		}
	}
	else if(nice->type == VIDEO_TYPE){
		if (video_receiver){
			if (video_srtp){
				length = video_srtp->UnprotectRtp(buf,len);
			}
			video_receiver->receiveVideoData(buf, length);
		}
	}
	return -1;
}
std::string WebRTCConnection::getLocalAddress(){
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa ->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
            if (!strcmp(ifa->ifa_name, "eth0")){
            	return std::string(addressBuffer);
            }

        }
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    return 0;

}



