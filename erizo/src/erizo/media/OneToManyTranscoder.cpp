/*
 * OneToManyTranscoder.cpp
 */

#include "OneToManyTranscoder.h"
#include "../WebRtcConnection.h"
#include "../rtp/RtpHeaders.h"

namespace erizo {
DEFINE_LOGGER(OneToManyTranscoder, "media.OneToManyTranscoder");
OneToManyTranscoder::OneToManyTranscoder() {


	publisher = NULL;
	sentPackets_ = 0;
    ip_ = new InputProcessor();
	MediaInfo m;
	m.processorType = RTP_ONLY;
//	m.videoCodec.bitRate = 2000000;
//	ELOG_DEBUG("m.videoCodec.bitrate %d\n", m.videoCodec.bitRate);
	m.hasVideo = true;
	m.videoCodec.width = 640;
	m.videoCodec.height = 480;
	m.hasAudio = false;

  ELOG_DEBUG("init ip");
    ip_->init(m, this);

	MediaInfo om;
	om.processorType = RTP_ONLY;
	om.videoCodec.bitRate = 2000000;
	om.videoCodec.width = 640;
	om.videoCodec.height = 480;
	om.videoCodec.frameRate = 20;
	om.hasVideo = true;
//	om.url = "file://tmp/test.mp4";

	om.hasAudio = false;

    op_ = new OutputProcessor();
    op_->init(om, this);

}

OneToManyTranscoder::~OneToManyTranscoder() {
	this->closeAll();
    delete ip_; ip_ = NULL;
    delete op_; op_ = NULL;
}

int OneToManyTranscoder::deliverAudioData_(char* buf, int len) {
	if (subscribers.empty() || len <= 0)
		return 0;

	std::map<std::string, MediaSink*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		memcpy(sendAudioBuffer_, buf, len);
		(*it).second->deliverAudioData(sendAudioBuffer_, len);
	}

	return 0;
}

int OneToManyTranscoder::deliverVideoData_(char* buf, int len) {
	memcpy(sendVideoBuffer_, buf, len);

	RtpHeader* theHead = reinterpret_cast<RtpHeader*>(buf);
//	ELOG_DEBUG("extension %d pt %u", theHead->getExtension(),
//			theHead->getPayloadType());

	if (theHead->getPayloadType() == 100) {
        ip_->deliverVideoData(sendVideoBuffer_, len);
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
//	ELOG_DEBUG("Received %d", pkt.length);
    op_->receiveRawData(pkt);
}

void OneToManyTranscoder::receiveRtpData(unsigned char*rtpdata, int len) {
	ELOG_DEBUG("Received rtp data %d", len);
	memcpy(sendVideoBuffer_, rtpdata, len);

	if (subscribers.empty() || len <= 0)
		return;
//	if (sentPackets_ % 500 == 0) {
//		publisher->sendFirPacket();
//	}
	std::map<std::string, MediaSink*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		(*it).second->deliverVideoData(sendVideoBuffer_, len);
	}
	sentPackets_++;
}

void OneToManyTranscoder::setPublisher(MediaSource* webRtcConn) {
	this->publisher = webRtcConn;
}

void OneToManyTranscoder::addSubscriber(MediaSink* webRtcConn,
		const std::string& peerId) {
	this->subscribers[peerId] = webRtcConn;
}

  void OneToManyTranscoder::removeSubscriber(const std::string& peerId) {
    if (this->subscribers.find(peerId) != subscribers.end()) {
      delete this->subscribers[peerId];      
      this->subscribers.erase(peerId);
    }
  }

  void OneToManyTranscoder::closeAll() {
      ELOG_WARN ("OneToManyTranscoder closeAll");
      std::map<std::string, MediaSink*>::iterator it = subscribers.begin();
      while( it != subscribers.end()) {
        delete (*it).second;
        it = subscribers.erase(it);
      }
    delete this->publisher;
  }

}/* namespace erizo */

