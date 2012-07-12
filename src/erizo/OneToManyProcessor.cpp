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
	ip->init(m, this);
	// Media processing

//	unpackagedBuffer_ = (char*) malloc(50000);
//	memset(unpackagedBuffer_, 0, 50000);
//
//	gotFrame_ = 0;
//	size_ = 0;
//	gotDecodedFrame_ = 0;
//
//	mp = new MediaProcessor();
//	videoCodecInfo *v = new videoCodecInfo;
//	v->codec = CODEC_ID_VP8;
//	//	v->codec = CODEC_ID_MPEG4;
//	v->width = 640;
//	v->height = 480;
//	decodedBuffer_ = (char*) malloc(v->width * v->height * 3 / 2);
//	memset(decodedBuffer_, 0, v->width * v->height * 3 / 2);
//	mp->initVideoDecoder(v);
//
//	videoCodecInfo *c = new videoCodecInfo;
//	//c->codec = CODEC_ID_MPEG2VIDEO;
//	c->codec = CODEC_ID_H263P;
//	c->width = v->width;
//	c->height = v->height;
//	c->frameRate = 24;
//	c->bitRate = 1024;
//	c->maxInter = 0;
//
//	mp->initVideoCoder(c);
//
//	RTPInfo *r = new RTPInfo;
//	r->codec = CODEC_ID_H263P;
//
//	mp->initVideoPackagerRTP(r);
//	mp->initVideoUnpackagerRTP(r);

}

OneToManyProcessor::~OneToManyProcessor() {

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

////	if (subscribers.empty() || len <= 0)
////		return 0;
////	if (sentPackets_ % 1000 == 0) {
////		publisher->sendFirPacket();
////	}
////	std::map<int, WebRtcConnection*>::iterator it;
////	for (it = subscribers.begin(); it != subscribers.end(); it++) {
////		memset(sendVideoBuffer_, 0, len);
////		memcpy(sendVideoBuffer_, buf, len);
////		(*it).second->receiveVideoData(sendVideoBuffer_, len);
////	}
////	sentPackets_++;
//	return 0;
}

void OneToManyProcessor::receiveRawData(unsigned char* buf, int len){
	printf("Received %d", len);
}
void OneToManyProcessor::setPublisher(WebRtcConnection* webRtcConn) {

	this->publisher = webRtcConn;
}

void OneToManyProcessor::addSubscriber(WebRtcConnection* webRtcConn,
		int peerId) {
	this->subscribers[peerId] = webRtcConn;
//	sendHead(webRtcConn);

}

void OneToManyProcessor::removeSubscriber(int peerId) {

	if (this->subscribers.find(peerId) != subscribers.end()) {
		this->subscribers[peerId]->close();
		this->subscribers.erase(peerId);
	}
}

}/* namespace erizo */

