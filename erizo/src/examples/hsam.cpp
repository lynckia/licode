/*
 * hsam.cpp
 */

#include <stdio.h>

#include <OneToManyProcessor.h>
#include <SdpInfo.h>
#include <WebRtcConnection.h>
#include <LibNiceConnection.h>
#include "Test.h"
#include "pc/Observer.h"

using namespace erizo;

int publisherid = 0;
int main() {

	new Test();

//	SDPReceiver* receiver = new SDPReceiver();
//	Observer *subscriber = new Observer("subscriber", receiver);
//	new Observer("publisher", receiver);
//	subscriber->wait();
//	return 0;
}

SDPReceiver::SDPReceiver() {
	muxer = new erizo::OneToManyProcessor();
}

bool SDPReceiver::createPublisher(int peer_id) {
	if (muxer->publisher == NULL) {
		printf("Adding publisher peer_id %d\n", peer_id);
		WebRtcConnection *newConn = new WebRtcConnection;
		newConn->init();
		newConn->setAudioReceiver(muxer);
		newConn->setVideoReceiver(muxer);
		muxer->setPublisher(newConn);
		publisherid = peer_id;
	} else {
		printf("PUBLISHER ALREADY SET\n");
		return false;
	}
	return true;
}
bool SDPReceiver::createSubscriber(int peer_id) {
	printf("Adding Subscriber peerid %d\n", peer_id);
	if (muxer->subscribers.find(peer_id) != muxer->subscribers.end()) {
		printf("OFFER AGAIN\n");
		return false;
	}

	WebRtcConnection *newConn = new WebRtcConnection;
	newConn->init();
	muxer->addSubscriber(newConn, peer_id);
	return true;
}
void SDPReceiver::setRemoteSDP(int peer_id, const std::string &sdp) {
	if (peer_id == publisherid) {
		muxer->publisher->setRemoteSdp(sdp);

	} else {
		muxer->subscribers[peer_id]->setRemoteSdp(sdp);
	}
}
std::string SDPReceiver::getLocalSDP(int peer_id) {
	std::string sdp;
	if (peer_id == publisherid) {
		sdp = muxer->publisher->getLocalSdp();
	} else {
		sdp = muxer->subscribers[peer_id]->getLocalSdp();
	}
	printf("Getting localSDP %s\n", sdp.c_str());
	return sdp;
}
void SDPReceiver::peerDisconnected(int peer_id) {
	if (peer_id != publisherid) {
		printf("removing peer %d\n", peer_id);
		muxer->removeSubscriber(peer_id);
	}
}

