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
	ip = new InputProcessor();
	MediaInfo m;
	m.proccessorType= RTP_ONLY;
	m.videoCodec.bitRate =2000000;
	printf("m.videoCodec.bitrate %d\n\n", m.videoCodec.bitRate);
	m.videoCodec.height = 480;
	m.videoCodec.width = 640;
	ip->init(m, this);


	MediaInfo om;
	om.proccessorType = RTP_ONLY;
	om.videoCodec.bitRate =2000000;
	printf("m.videoCodec.bitrate %d\n\n", om.videoCodec.bitRate);
	om.videoCodec.height = 480;
	om.videoCodec.width = 640;
	printf("Initiation outputprocessor\n");
	op = new OutputProcessor();
	op->init(om);
	printf("OutputProcessor initiated successfully\n");

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

	std::map<int, WebRtcConnection*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		memset(sendAudioBuffer_, 0, len);
		memcpy(sendAudioBuffer_, buf, len);
		(*it).second->receiveAudioData(sendAudioBuffer_, len);
	}

	return 0;
}

int OneToManyProcessor::receiveVideoData(char* buf, int len) {

	ip->receiveVideoData(buf,len);

//	if (subscribers.empty() || len <= 0)
//		return 0;
//	if (sentPackets_ % 500 == 0) {
//		publisher->sendFirPacket();
//	}
//	std::map<int, WebRtcConnection*>::iterator it;
//	for (it = subscribers.begin(); it != subscribers.end(); it++) {
//		memset(sendVideoBuffer_, 0, len);
//		memcpy(sendVideoBuffer_, buf, len);
//		(*it).second->receiveVideoData(sendVideoBuffer_, len);
//	}
//	sentPackets_++;
//	return 0;
}

void OneToManyProcessor::receiveRawData(unsigned char* buf, int len){
	printf("Received %d\n", len);
//	free(buf);
}
void OneToManyProcessor::setPublisher(WebRtcConnection* webRtcConn) {

	this->publisher = webRtcConn;
}

void OneToManyProcessor::addSubscriber(WebRtcConnection* webRtcConn,
		int peerId) {
	this->subscribers[peerId] = webRtcConn;

}

void OneToManyProcessor::removeSubscriber(int peerId) {

	if (this->subscribers.find(peerId) != subscribers.end()) {
		this->subscribers[peerId]->close();
		this->subscribers.erase(peerId);
	}
}

void OneToManyProcessor::closeAll() {
	std::map<int, WebRtcConnection*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		(*it).second->close();
	}
	this->publisher->close();
}

}/* namespace erizo */

