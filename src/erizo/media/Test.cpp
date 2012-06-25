#include "Test.h"

Test::Test() {
	// TODO Auto-generated constructor stub
	mp = new MediaProcessor();



//	audioCodecInfo *i = new audioCodecInfo;
//	i->codec = CODEC_ID_AMR_NB;
//	i->bitRate = 300000;
//	i->sampleRate = 44100;
//	mp->initAudioDecoder(i);

//	RTPInfo *r = new RTPInfo;
//	r->codec = CODEC_ID_MP3;
//	mp->initAudioUnpackagerRTP(r);


	videoCodecInfo *v = new videoCodecInfo;
	v->codec = CODEC_ID_MPEG2VIDEO;
	//v->codec = CODEC_ID_MPEG4;
	v->width = 176;
	v->height = 144;

	mp->initVideoDecoder(v);

	videoCodecInfo *c = new videoCodecInfo;
	//c->codec = CODEC_ID_MPEG2VIDEO;
	c->codec = CODEC_ID_H263;
	c->width = 176;
	c->height = 144;
	c->frameRate = 24;
	c->bitRate = 1024;
	c->maxInter = 0;

	mp->initVideoCoder(c);

	RTPInfo *r = new RTPInfo;
	//r->codec = CODEC_ID_MPEG2VIDEO;
//	r->codec = CODEC_ID_MPEG4;
	mp->initVideoPackagerRTP(r);
	mp->initVideoUnpackagerRTP(r);


//	sock = new UDPSocket(5004);
	boost::thread t = boost::thread(&Test::rec, this);
	t.join();
}

Test::~Test() {
	// TODO Auto-generated destructor stub
	//sock->disconnect();
}

void Test::rec() {

	int outBuff2Size = 176*144*3/2;

    void *buff = malloc(2000);
    char * outBuff = (char*)malloc(50000);
    memset(outBuff, 0, 50000);
    char * outBuff2 = (char*)malloc(outBuff2Size);

    int gotFrame = 0;
    int size = 0;
    int gotDecFrame = 0;

    std::string s;
    unsigned short u;

    while(true) {

    	memset(buff, 0, 2000);

//    	int a = sock->recvFrom(buff, 2000, s, u);

    	printf("\n********* RECEPCIÓN *********");
    	int a = 5;
		printf("\n\nBytes = %d", a);

		int b = mp->unpackageVideoRTP((char*)buff, a, outBuff, &gotFrame);

		printf("\nBytes desem = %d", b);

		size += b;
		outBuff += b;

		if(gotFrame) {

			outBuff -= size;

			printf("\nTengo un frame desempaquetado!! Size = %d", size);

			int c;

			c = mp->decodeVideo(outBuff, size, outBuff2, outBuff2Size, &gotDecFrame);
			printf("\nBytes dec = %d", c);

			printf("\n*****************************");

			size = 0;
			memset(outBuff, 0, 50000);
			gotFrame = 0;

			if(gotDecFrame) {
				printf("\nTengo un frame decodificado!!");
				gotDecFrame = 0;
				send(outBuff2, c);
			}
		}
    }

}

void Test::send(char *buff, int buffSize) {

	printf("\n********* ENVÍO *********");

	char *buffSend = (char*)malloc(buffSize);
	char *buffSend2 = (char*)malloc(buffSize);

	int a = mp->encodeVideo(buff, buffSize, buffSend, buffSize);

	printf("\nBytes codificados = %d", a);

	int b = mp->packageVideoRTP(buffSend, a, buffSend2);

	printf("\nBytes empaquetados = %d", b);

	//sock->sendTo(buffSend2, b, "toronado.dit.upm.es", 5005);

	free(buffSend);
	free(buffSend2);

	printf("\n*************************");
}

