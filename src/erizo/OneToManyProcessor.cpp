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

	// Media processing

    unpackagedBuffer_ = (char*)malloc(50000);
    memset(unpackagedBuffer_, 0, 50000);


    gotFrame_ = 0;
    size_ = 0;
    gotDecodedFrame_=0;



	mp = new MediaProcessor();
	videoCodecInfo *v = new videoCodecInfo;
	v->codec = CODEC_ID_VP8;
	//	v->codec = CODEC_ID_MPEG4;
	v->width = 640;
	v->height = 480;
	decodedBuffer_ = (char*) malloc(v->width*v->height*3/2);
	memset(decodedBuffer_,0,v->width*v->height*3/2);
	mp->initVideoDecoder(v);

	videoCodecInfo *c = new videoCodecInfo;
	//c->codec = CODEC_ID_MPEG2VIDEO;
	c->codec = CODEC_ID_VP8;
	c->width = v->width;
	c->height = v->height;
	c->frameRate = 24;
	c->bitRate = 1024;
	c->maxInter = 0;

	mp->initVideoCoder(c);

	RTPInfo *r = new RTPInfo;
	//r->codec = CODEC_ID_MPEG2VIDEO;
	//	r->codec = CODEC_ID_MPEG4;
	mp->initVideoPackagerRTP(r);
	mp->initVideoUnpackagerRTP(r);

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

	int b;
	int x = mp->unpackageVideoRTP(buf,len,unpackagedBuffer_,&gotFrame_);
//	if (x>0){
//		RTPPayloadVP8* parsed = pars.parseVP8((unsigned char*)unpackagedBuffer_, x);
//		b = parsed->dataLength;
//		printf("Bytes desem = %d, sobre %d reciv\n", b, len);
//
//	}
	size_ += x;
	unpackagedBuffer_ += x;
	if(gotFrame_) {

				unpackagedBuffer_ -= size_;

				printf("Tengo un frame desempaquetado!! Size = %d\n", size_);

				int c;

				c = mp->decodeVideo(unpackagedBuffer_, size_, decodedBuffer_, 640*480*3/2, &gotDecodedFrame_);
				printf("Bytes dec = %d\n", c);

				printf("\n*****************************");

				size_ = 0;
				memset(unpackagedBuffer_, 0, 50000);
				gotFrame_ = 0;

				if(gotDecodedFrame_) {
					printf("\nTengo un frame decodificado!!");
					gotDecodedFrame_ = 0;
					//send(outBuff2, c);
				}
			}
	if (subscribers.empty() || len <= 0)
		return 0;

	std::map<int, WebRtcConnection*>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		memset(sendVideoBuffer_, 0, len);
		memcpy(sendVideoBuffer_, buf, len);
		(*it).second->receiveVideoData(sendVideoBuffer_, len);
	}
	return 0;
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

}/* namespace erizo */

