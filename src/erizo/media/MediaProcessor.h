#ifndef MEDIAPROCESSOR_H_
#define MEDIAPROCESSOR_H_

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>


extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>

}

struct audioCodecInfo {
	enum CodecID codec;
	int bitRate;
	int sampleRate;
};

struct videoCodecInfo {
	enum CodecID codec;
	int width;
	int height;
	int bitRate;
	int frameRate;
	int maxInter;
};

struct RTPInfo {
	enum CodecID codec;
	int seqNum;
	int ssrc;
};

typedef struct
{
	uint32_t cc:4;
	uint32_t extension:1;
	uint32_t padding:1;
	uint32_t version:2;
	uint32_t payloadtype:7;
	uint32_t marker:1;
	uint32_t seqnum:16;
	uint32_t timestamp;
	uint32_t ssrc;
}rtpHeader;

typedef struct
{
	uint32_t partId:4;
	uint32_t S:1;
	uint32_t N:1;
	uint32_t R:1;
	uint32_t X:1;
}vp8RtpHeader;


#define RTP_HEADER_LEN 12;

class MediaProcessor {
public:
	MediaProcessor();
	virtual ~MediaProcessor();

	bool initAudioCoder(const audioCodecInfo *audioCodec);
	bool initAudioDecoder(const audioCodecInfo *audioCodec);
	bool initVideoCoder(const videoCodecInfo *videoCodec);
	bool initVideoDecoder(const videoCodecInfo *videoCodec);

	bool initAudioPackagerRTP(const RTPInfo *audioRTP);
	bool initAudioUnpackagerRTP(const RTPInfo *audioRTP);
	bool initVideoPackagerRTP(const RTPInfo *videoRTP);
	bool initVideoUnpackagerRTP(const RTPInfo *videoRTP);

	int encodeAudio(char *inBuff, int nSamples, char *outBuff);
	int decodeAudio(char *inBuff, int inBuffLen, char *outBuff);
	int encodeVideo(char *inBuff, int inBuffLen, char *outBuff, int outBuffLen);
	int decodeVideo(char *inBuff, int inBuffLen, char *outBuff, int outBuffLen, int *gotFrame);

	int packageAudioRTP(char *inBuff, int inBuffLen, char *outBuff);
	int unpackageAudioRTP(char *inBuff, int inBuffLen, char *outBuff);
	int packageVideoRTP(char *inBuff, int inBuffLen, char *outBuff);
	int unpackageVideoRTP(char *inBuff, int inBuffLen, char *outBuff, int *gotFrame);

	//readFromFile
	//writeTofile
	//Encapsular
	//Desencapsular

private:
	int audioCoder;
	int audioDecoder;
	int videoCoder;
	int videoDecoder;

	int audioPackager;
	int audioUnpackager;
	int videoPackager;
	int videoUnpackager;

	AVCodec *aCoder;
	AVCodecContext *aCoderContext;
	AVCodec *aDecoder;
	AVCodecContext *aDecoderContext;

	AVCodec *vCoder;
	AVCodecContext *vCoderContext;
	AVFrame *cPicture;
	AVCodec *vDecoder;
	AVCodecContext *vDecoderContext;
	AVFrame *dPicture;

	AVFormatContext *aInputFormatContext;
	AVInputFormat *aInputFormat;
	AVFormatContext *aOutputFormatContext;
	AVOutputFormat *aOutputFormat;

	RTPInfo *vRTPInfo;

	AVFormatContext *vInputFormatContext;
	AVInputFormat *vInputFormat;
	AVFormatContext *vOutputFormatContext;
	AVOutputFormat *vOutputFormat;

};

#endif /* MEDIAPROCESSOR_H_ */
