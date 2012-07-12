#include <iostream>
#include <stdio.h>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "Test.h"

using boost::asio::ip::udp;

namespace erizo {
Test::Test() {
	ip = new InputProcessor();
	MediaInfo m;
	ip->init(m, this);
//	mp = new MediaProcessor();
//
////	audioCodecInfo *i = new audioCodecInfo;
////	i->codec = CODEC_ID_AMR_NB;
////	i->bitRate = 300000;
////	i->sampleRate = 44100;
////	mp->initAudioDecoder(i);
//
////	RTPInfo *r = new RTPInfo;
////	r->codec = CODEC_ID_MP3;
////	mp->initAudioUnpackagerRTP(r);
//
//	videoCodecInfo *v = new videoCodecInfo;
//	v->codec = CODEC_ID_VP8;
////	v->codec = CODEC_ID_MPEG4;
//	//v->width = 706;
//	//v->height = 396;
//	v->width = 400;
//	v->height = 300;
//
//	mp->initVideoDecoder(v);
//
//	videoCodecInfo *c = new videoCodecInfo;
//	//c->codec = CODEC_ID_MPEG2VIDEO;
//	c->codec = CODEC_ID_MPEG2VIDEO;
//	c->width = v->width;
//	c->height = v->height;
//	c->frameRate = 24;
//	c->bitRate = 1024;
//	c->maxInter = 0;
//
//	mp->initVideoCoder(c);
//
//	RTPInfo *r = new RTPInfo;
//	//r->codec = CODEC_ID_MPEG2VIDEO;
////	r->codec = CODEC_ID_MPEG4;
//	mp->initVideoPackagerRTP(r);
//	mp->initVideoUnpackagerRTP(r);
//
	ioservice_ = new boost::asio::io_service;
	resolver_ = new udp::resolver(*ioservice_);
	socket_ = new udp::socket(*ioservice_, udp::endpoint(udp::v4(), 40000));
	query_ = new udp::resolver::query(udp::v4(), "127.0.0.1", "50000");

	boost::thread t = boost::thread(&Test::rec, this);
	t.join();

}

Test::~Test() {
	//sock->disconnect();
}

void Test::receiveRawData(unsigned char*data, int len) {
	printf("decoded data %d\n", len);
	return;
}

void Test::rec() {

//	int outBuff2Size = 706 * 396 * 3 / 2;
//
	char* buff = (char*)malloc(2000);
//	char * outBuff = (char*) malloc(50000);
//	memset(outBuff, 0, 50000);
//	char * outBuff2 = (char*) malloc(outBuff2Size);
//
//	int gotFrame = 0;
//	int size = 0;
//	int gotDecFrame = 0;
//
//	std::string s;
//	unsigned short u;
	int a;
	while (true) {
//
		memset(buff, 0, 2000);
//
		a = socket_->receive(boost::asio::buffer(buff, 2000));
		ip->receiveVideoData(buff, a);
//		printf("********* RECEPCIÓN *********\n");
//		printf("Bytes = %d\n", a);
//
//		int z, b;
//		z = mp->unpackageVideoRTP((char*) buff, a, outBuff, &gotFrame);
//
////		RTPPayloadVP8* parsed = pars.parseVP8((unsigned char*)outBuff, z);
////		b = parsed->dataLength;
////		int c = parsed.dataLength;
//
////		printf("Bytes desem = %d prevp8 %d\n", b, z );
//
//		size += z;
//		outBuff += z;
//
//		if (gotFrame) {
//
//			outBuff -= size;
//
//			printf("Tengo un frame desempaquetado!! Size = %d\n", size);
//
//			int c;
//
//			c = mp->decodeVideo(outBuff, size, outBuff2, outBuff2Size,
//					&gotDecFrame);
//			printf("Bytes dec = %d\n", c);
//
//			size = 0;
//			memset(outBuff, 0, 50000);
//			gotFrame = 0;
//
//			if (gotDecFrame) {
//				printf("Tengo un frame decodificado!!");
//				gotDecFrame = 0;
//				send(outBuff2, c);
//			}
	}
//	}

}

void Test::send(char *buff, int buffSize) {

//	printf("\n********* ENVÍO *********");
//
//	char *buffSend = (char*) malloc(buffSize);
//	char *buffSend2 = (char*) malloc(buffSize);
//
//	int a = mp->encodeVideo(buff, buffSize, buffSend, buffSize);
//
//	printf("\nBytes codificados = %d", a);
//
//	int b = mp->packageVideoRTP(buffSend, a, buffSend2);
//
//	printf("\nBytes empaquetados = %d", b);
//
////	udp::resolver::iterator iterator = resolver_->resolve(*query_);
//
////	socket_->send_to(buffSend2, b, "toronado.dit.upm.es", 5005);
////	socket_->send_to(boost::asio::buffer(buffSend2, b), *iterator);
//	free(buffSend);
//	free(buffSend2);
//
//	printf("\n*************************");
}

}
