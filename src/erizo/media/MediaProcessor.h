#ifndef MEDIAPROCESSOR_H_
#define MEDIAPROCESSOR_H_

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>
#include <string>

#include "utils/RtpParser.h"
#include "../MediaDefinitions.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

}

namespace erizo {

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
	unsigned int ssrc;
	unsigned int PT;
};

enum ProcessorType {
	RTP_ONLY, AVF
};

struct MediaInfo {
	std::string url;
	bool hasVideo;
	bool hasAudio;
	ProcessorType proccessorType;
	RTPInfo rtpVideoInfo;
	RTPInfo rtpAudioInfo;
	videoCodecInfo videoCodec;
	audioCodecInfo audioCodec;

};

typedef struct {
	uint32_t cc :4;
	uint32_t extension :1;
	uint32_t padding :1;
	uint32_t version :2;
	uint32_t payloadtype :7;
	uint32_t marker :1;
	uint32_t seqnum :16;
	uint32_t timestamp;
	uint32_t ssrc;
	uint32_t extId:16;
	uint32_t extLength:16;
} rtpHeader;

typedef struct {
	uint32_t partId :4;
	uint32_t S :1;
	uint32_t N :1;
	uint32_t R :1;
	uint32_t X :1;
} vp8RtpHeader;

#define RTP_HEADER_LEN 12

#define UNPACKAGED_BUFFER_SIZE 50000
//class MediaProcessor{
//	MediaProcessor();
//	virtual ~Mediaprocessor();
//private:
//	InputProcessor* input;
//	OutputProcessor* output;
//};

class RawDataReceiver {
public:
	virtual void receiveRawData(unsigned char*data, int len) = 0;
	virtual ~RawDataReceiver() {
	}
	;

};

class InputProcessor: MediaReceiver {
public:
	InputProcessor();
	virtual ~InputProcessor();

	int init(const MediaInfo& info, RawDataReceiver* receiver);

	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);

private:

	int audioDecoder;
	int videoDecoder;

	MediaInfo mediaInfo;

	int audioUnpackager;
	int videoUnpackager;

	int gotUnpackagedFrame_;
	int upackagedSize_;

	unsigned char* decodedBuffer_;
	unsigned char* unpackagedBuffer_;


	AVCodec* aDecoder;
	AVCodecContext* aDecoderContext;

	AVCodec* vDecoder;
	AVCodecContext* vDecoderContext;
	AVFrame* dPicture;

	AVFormatContext* aInputFormatContext;
	AVInputFormat* aInputFormat;

	RTPInfo* vRTPInfo;

	AVFormatContext* vInputFormatContext;
	AVInputFormat* vInputFormat;

	RawDataReceiver* rawReceiver_;

	erizo::RtpParser pars;

	bool initAudioDecoder();
	bool initVideoDecoder();

	bool initAudioUnpackager();
	bool initVideoUnpackager();

	int decodeAudio(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff);
	int decodeVideo(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff, int outBuffLen, int* gotFrame);

	int unpackageAudio(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff);
	int unpackageVideo(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff, int* gotFrame);

};
class OutputProcessor: public RawDataReceiver {
public:

	OutputProcessor();
	virtual ~OutputProcessor();
	int init(const MediaInfo &info);

	void receiveRawData(unsigned char*data, int len);

private:

	int audioCoder;
	int videoCoder;

	int audioPackager;
	int videoPackager;

	unsigned char* encodedBuffer_;
	unsigned char* packagedBuffer_;

	MediaInfo mediaInfo;

	AVCodec* aCoder;
	AVCodecContext* aCoderContext;

	AVCodec* vCoder;
	AVCodecContext* vCoderContext;
	AVFrame* cPicture;

	AVFormatContext* aOutputFormatContext;
	AVOutputFormat* aOutputFormat;

	RTPInfo* vRTPInfo;

	AVFormatContext* vOutputFormatContext;
	AVOutputFormat* vOutputFormat;

	MediaReceiver* Receiver;

	RtpParser pars;

	bool initAudioCoder();
	bool initVideoCoder();

	bool initAudioPackager();
	bool initVideoPackager();

	int encodeAudio(unsigned char* inBuff, int nSamples,
			unsigned char* outBuff);
	int encodeVideo(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff, int outBuffLen);

	int encodeVideo(unsigned char* inBuff, int inBuffLen, AVPacket* pkt);

	int packageAudio(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff);
	int packageVideo(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff);
	int packageVideo(AVPacket* pkt, unsigned char* outBuff);
};
} /* namespace erizo */

#endif /* MEDIAPROCESSOR_H_ */
