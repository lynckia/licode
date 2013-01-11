/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"
#include "WebRtcConnection.h"

namespace erizo {
OneToManyProcessor::OneToManyProcessor() :
		MediaReceiver() {

	sendVideoBuffer_ = (char*) malloc(2000);
	sendAudioBuffer_ = (char*) malloc(2000);
	publisher = NULL;
	sentPackets_ = 0;

}

OneToManyProcessor::~OneToManyProcessor() {
	this->closeAll();
	if (sendVideoBuffer_)
		delete sendVideoBuffer_;
	if (sendAudioBuffer_)
		delete sendAudioBuffer_;
}

int OneToManyProcessor::receiveAudioData(char* buf, int len) {

	if (subscribers.empty() || len <= 0)
		return 0;

	std::map<std::string, WebRtcConnection*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		memset(sendAudioBuffer_, 0, len);
		memcpy(sendAudioBuffer_, buf, len);
		(*it).second->receiveAudioData(sendAudioBuffer_, len);
	}

	return 0;
}

int OneToManyProcessor::receiveVideoData(char* buf, int len) {
	if (subscribers.empty() || len <= 0)
		return 0;
/*	if (sentPackets_ % 500 == 0) {
		publisher->sendFirPacket();
	}
  */
	std::map<std::string, WebRtcConnection*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		memset(sendVideoBuffer_, 0, len);
		memcpy(sendVideoBuffer_, buf, len);
		(*it).second->receiveVideoData(sendVideoBuffer_, len);
	}
	sentPackets_++;
	return 0;
}

void OneToManyProcessor::setPublisher(WebRtcConnection* webRtcConn) {

	this->publisher = webRtcConn;
}

void OneToManyProcessor::addSubscriber(WebRtcConnection* webRtcConn,
		const std::string& peerId) {
  printf("Adding subscriber\n");
  webRtcConn->localAudioSsrc_ = this->publisher->remoteAudioSSRC_;
  webRtcConn->localVideoSsrc_ = this->publisher->remoteVideoSSRC_;
	this->subscribers[peerId] = webRtcConn;
}

void OneToManyProcessor::removeSubscriber(const std::string& peerId) {
	if (this->subscribers.find(peerId) != subscribers.end()) {
		this->subscribers[peerId]->close();
		this->subscribers.erase(peerId);
	}
}

void OneToManyProcessor::closeAll() {
	std::map<std::string, WebRtcConnection*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		(*it).second->close();
	}
	this->publisher->close();
}

}/* namespace erizo */

