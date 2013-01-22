/*
 * OneToManyTranscoder.cpp
 */

#include "OneToManyTranscoder.h"
#include "../WebRtcConnection.h"
#include "../RTPSink.h"
#include "rtp/RtpHeader.h"

namespace erizo {
OneToManyTranscoder::OneToManyTranscoder() {

	sendVideoBuffer_ = (char*) malloc(2000);
	sendAudioBuffer_ = (char*) malloc(2000);

	publisher = NULL;
	sentPackets_ = 0;
	ip = new InputProcessor();
	sink_ = new RTPSink("127.0.0.1", "50000");
	MediaInfo m;
	m.proccessorType = RTP_ONLY;
//	m.videoCodec.bitRate = 2000000;
//	printf("m.videoCodec.bitrate %d\n\n", m.videoCodec.bitRate);
	m.hasVideo = true;
	m.videoCodec.width = 640;
	m.videoCodec.height = 480;
	m.hasAudio = false;
	if (m.hasAudio) {
		m.audioCodec.sampleRate = 8000;
		m.audioCodec.bitRate = 64000;

	}
  printf("init ip\n");
	ip->init(m, this);

	MediaInfo om;
	om.proccessorType = RTP_ONLY;
	om.videoCodec.bitRate = 2000000;
	om.videoCodec.width = 640;
	om.videoCodec.height = 480;
	om.videoCodec.frameRate = 20;
	om.hasVideo = true;
//	om.url = "file://tmp/test.mp4";

	om.hasAudio = false;
	if (om.hasAudio) {
		om.audioCodec.sampleRate = 8000;
		om.audioCodec.bitRate = 64000;
	}

	op = new OutputProcessor();
	op->init(om, this);

}

OneToManyTranscoder::~OneToManyTranscoder() {
	this->closeAll();
	if (sendVideoBuffer_)
		delete sendVideoBuffer_;
	if (sendAudioBuffer_)
		delete sendAudioBuffer_;
	if (sink_) {
		delete sink_;
	}
}

int OneToManyTranscoder::receiveAudioData(char* buf, int len) {

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

int OneToManyTranscoder::receiveVideoData(char* buf, int len) {
	memset(sendVideoBuffer_, 0, len);
	memcpy(sendVideoBuffer_, buf, len);

	RTPHeader* theHead = reinterpret_cast<RTPHeader*>(buf);
//	printf("extension %d pt %u\n", theHead->getExtension(),
//			theHead->getPayloadType());

	if (theHead->getPayloadType() == 100) {
		ip->receiveVideoData(sendVideoBuffer_, len);
	} else {
		this->receiveRtpData((unsigned char*) buf, len);
	}

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
//	memset(sendVideoBuffer_, 0, len);
//	memcpy(sendVideoBuffer_, buf, len);
//	sink_->sendData((unsigned char*)sendVideoBuffer_,len);

	sentPackets_++;
	return 0;
}

void OneToManyTranscoder::receiveRawData(RawDataPacket& pkt) {
//	printf("Received %d\n", pkt.length);
	op->receiveRawData(pkt);
}

void OneToManyTranscoder::receiveRtpData(unsigned char*rtpdata, int len) {
	printf("Received rtp data %d\n", len);
	memcpy(sendVideoBuffer_, rtpdata, len);

	if (subscribers.empty() || len <= 0)
		return;
//	if (sentPackets_ % 500 == 0) {
//		publisher->sendFirPacket();
//	}
	std::map<std::string, WebRtcConnection*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		memcpy(sendVideoBuffer_, rtpdata, len);
		(*it).second->receiveVideoData(sendVideoBuffer_, len);
	}
	sentPackets_++;
}

void OneToManyTranscoder::setPublisher(WebRtcConnection* webRtcConn) {
	this->publisher = webRtcConn;
}

void OneToManyTranscoder::addSubscriber(WebRtcConnection* webRtcConn,
		const std::string& peerId) {
	this->subscribers[peerId] = webRtcConn;
}

void OneToManyTranscoder::removeSubscriber(const std::string& peerId) {
	if (this->subscribers.find(peerId) != subscribers.end()) {
		this->subscribers[peerId]->close();
		this->subscribers.erase(peerId);
	}
}

void OneToManyTranscoder::close() {
  this->closeAll();
}

void OneToManyTranscoder::closeAll() {
	std::map<std::string, WebRtcConnection*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		(*it).second->close();
	}
	this->publisher->close();
}

}/* namespace erizo */

