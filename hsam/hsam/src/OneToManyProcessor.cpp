/*
 * OneToManyProcessor.cpp
 *
 *  Created on: Mar 21, 2012
 *      Author: pedro
 */

#include "OneToManyProcessor.h"
#include "WebRTCConnection.h"
//#include <cstring>

OneToManyProcessor::OneToManyProcessor() : MediaReceiver(){
	sendVideoBuffer = (char*)malloc(2000);
	sendAudioBuffer = (char*)malloc(2000);
	// TODO Auto-generated constructor stub

}

OneToManyProcessor::~OneToManyProcessor() {

	// TODO Auto-generated destructor stub
}
int OneToManyProcessor::receiveAudioData(char* buf, int len){

	if (subscribers.empty()||len<=0)
		return 0;

	for (unsigned int it = 0; it < subscribers.size(); it++) {
		memset(sendAudioBuffer,0,len);
		memcpy(sendAudioBuffer,buf, len);
		subscribers[it]->receiveAudioData(sendAudioBuffer, len);
	}
	return 0;
}
int OneToManyProcessor::receiveVideoData(char* buf, int len){
	if (subscribers.empty()||len<=0)
		return 0;
	for (unsigned int it = 0; it < subscribers.size(); it++) {
		memset(sendVideoBuffer,0,len);
		memcpy(sendVideoBuffer,buf, len);
		subscribers[it]->receiveVideoData(sendVideoBuffer, len);
	}
	return 0;
}

void OneToManyProcessor::setPublisher(WebRTCConnection* conn) {
	this->publisher = conn;
}

void OneToManyProcessor::addSubscriber(WebRTCConnection* conn) {
	this->subscribers.push_back(conn);
}


