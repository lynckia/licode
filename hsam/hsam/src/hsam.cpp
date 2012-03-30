//============================================================================
// Name        : hsam.cpp
// Author      : Pedro Rodriguez
// Version     :
// Copyright   :
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <stdio.h>
#include <fstream>
#include "NiceConnection.h"
#include "WebRTCConnection.h"
#include "sdpinfo.h"
#include <nice/nice.h>
#include <srtp/srtp.h>
#include "pc/Observer.h"
#include "OneToManyProcessor.h"


#include <boost/regex.hpp>

using namespace std;
std::map<int, WebRTCConnection*> peers;


int main() {
//
//	WebRTCConnection pepe(true);
//	printf("pWQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQepepepe\n");
//	pepe.init();
//	printf("Local SDP\n%s\n", pepe.getLocalSDP().c_str());
//
//	printf("push remote sdp\n");
//	getchar();
//	std::ifstream t("/home/pedro/workspace/webRTC/MCU/prototype/sdp");
//	std::string str((std::istreambuf_iterator<char>(t)),
//                 std::istreambuf_iterator<char>());
//	cout << str <<endl;
//	pepe.setAudioReceiver(&pepe);
//	pepe.setVideoReceiver(&pepe);
//	pepe.setRemoteSDP(str);
//


    SDPReceiver* receiver = new SDPReceiver();
    Observer *subscriber = new Observer("subscriber", receiver);
    new Observer("publisher", receiver);
    subscriber->wait();



    return 0;
}

SDPReceiver::SDPReceiver() {
	muxer = new OneToManyProcessor();
}
// AQUI

void SDPReceiver::createPublisher(int peer_id){
	if (muxer->publisher==NULL){
		printf("Adding publisher\n");
		WebRTCConnection *newConn = new WebRTCConnection;
		newConn->init();
		newConn->setAudioReceiver(muxer);
		newConn->setVideoReceiver(muxer);
		muxer->setPublisher(newConn);
		peers[peer_id] = newConn;
	}else{
		printf("PUBLISHER ALREADY SET\n");
	}

}
void SDPReceiver::createSubscriber(int peer_id){
	printf("Adding Subscriber\n");
	WebRTCConnection *newConn = new WebRTCConnection;
	newConn->init();
	muxer->addSubscriber(newConn);
	peers[peer_id] = newConn;
}
void SDPReceiver::setRemoteSDP(int peer_id, const std::string &sdp){
	 peers[peer_id]->setRemoteSDP(sdp);

}
std::string SDPReceiver::getLocalSDP(int peer_id){
	std::string sdp =  peers[peer_id]->getLocalSDP();
	printf("Getting localSDP %s\n", sdp.c_str());
	return sdp;
}


