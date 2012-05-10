/*
 * OneToManyProcessor.cpp
 *
 *  Created on: Mar 21, 2012
 *      Author: pedro
 */

#include "OneToManyProcessor.h"
#include "WebRTCConnection.h"

OneToManyProcessor::OneToManyProcessor() : MediaReceiver(){
	sendVideoBuffer = (char*)malloc(2000);
	sendAudioBuffer = (char*)malloc(2000);

	publisher = NULL;

}

OneToManyProcessor::~OneToManyProcessor() {
	if(sendVideoBuffer)
		delete sendVideoBuffer;
	if(sendAudioBuffer)
		delete sendAudioBuffer;
}
int OneToManyProcessor::receiveAudioData(char* buf, int len){

	if (subscribers.empty()||len<=0)
		return 0;

	std::map<int,WebRTCConnection*>::iterator it;
	for (it = subscribers.begin(); it!=subscribers.end(); it++) {
		memset(sendAudioBuffer,0,len);
		memcpy(sendAudioBuffer,buf, len);
		(*it).second->receiveAudioData(sendAudioBuffer, len);
	}

	return 0;
}
int OneToManyProcessor::receiveVideoData(char* buf, int len){
	if (subscribers.empty()||len<=0)
		return 0;

	std::map<int,WebRTCConnection*>::iterator it;
	for (it = subscribers.begin(); it!=subscribers.end(); it++) {
		memset(sendVideoBuffer,0,len);
		memcpy(sendVideoBuffer,buf, len);
		(*it).second->receiveVideoData(sendVideoBuffer, len);
	}
	return 0;
}

void OneToManyProcessor::setPublisher(WebRTCConnection* conn) {
	this->publisher = conn;
}

void OneToManyProcessor::addSubscriber(WebRTCConnection* conn, int peer_id) {
	this->subscribers[peer_id]=conn;
}
void OneToManyProcessor::removeSubscriber(int peer_id){
	if (this->subscribers.find(peer_id)!=subscribers.end()){
		this->subscribers[peer_id]->close();
		this->subscribers.erase(peer_id);
	}
}





