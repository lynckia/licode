//============================================================================
// Name        : hsam.cpp
// Author      : Pedro Rodriguez
// Version     :
// Copyright   :
// Description : Test Publisher
//============================================================================

#include <iostream>
#include <stdio.h>
#include <fstream>

#include <nice/nice.h>
#include <srtp/srtp.h>
#include <boost/regex.hpp>

#include "pc/Observer.h"
#include "OneToManyProcessor.h"
#include "media/Test.h"
#include "NiceConnection.h"
#include "WebRTCConnection.h"
#include "SdpInfo.h"



using namespace std;
std::map<int, WebRTCConnection*> peers;
int publisherid = 0;
int main() {

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
    SDPReceiver* receiver = new SDPReceiver();
    Observer *subscriber = new Observer("subscriber", receiver);
    new Observer("publisher", receiver);
    subscriber->wait();

//	std::ifstream t("/home/pedro/sdpnuevo.sdp");
//	std::string str((std::istreambuf_iterator<char>(t)),std::istreambuf_iterator<char>());
//	cout << str <<endl;
//	SDPInfo sdp;
//	sdp.initWithSDP(str);
//	printf("resultado %s\n",sdp.getSDP().c_str());


    return 0;
}

SDPReceiver::SDPReceiver(){
	muxer = new OneToManyProcessor();
}
// AQUI
void SDPReceiver::createPublisher(int peer_id){
	if (muxer->publisher==NULL){
		printf("Adding publisher peer_id %d\n", peer_id);
		WebRTCConnection *newConn = new WebRTCConnection;
		newConn->init();
		newConn->setAudioReceiver(muxer);
		newConn->setVideoReceiver(muxer);
		muxer->setPublisher(newConn);
//		peers[peer_id] = newConn;
		publisherid = peer_id;
	}else{
		printf("PUBLISHER ALREADY SET\n");
	}

}
void SDPReceiver::createSubscriber(int peer_id){
	printf("Adding Subscriber peerid %d\n", peer_id);
	WebRTCConnection *newConn = new WebRTCConnection;
	newConn->init();
	muxer->addSubscriber(newConn, peer_id);
//	peers[peer_id] = newConn;
}
void SDPReceiver::setRemoteSDP(int peer_id, const std::string &sdp){
	if (peer_id == publisherid){
		muxer->publisher->setRemoteSDP(sdp);

	}else{
		//peers[peer_id]->setRemoteSDP(sdp);
		muxer->subscribers[peer_id]->setRemoteSDP(sdp);
	}
}
std::string SDPReceiver::getLocalSDP(int peer_id){
	std::string sdp;// =  peers[peer_id]->getLocalSDP();
	if (peer_id ==publisherid){
		sdp = muxer->publisher->getLocalSDP();
	}else{
		sdp = muxer->subscribers[peer_id]->getLocalSDP();
	}
	printf("Getting localSDP %s\n", sdp.c_str());
	return sdp;
}
void SDPReceiver::peerDisconnected(int peer_id){
	if (peer_id!=publisherid){
		printf("removing peer %d\n",peer_id);
		muxer->removeSubscriber(peer_id);
	}
}


