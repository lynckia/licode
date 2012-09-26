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

enum DataType {
	VIDEO, AUDIO
};

struct RawDataPacket {
	unsigned char* data;
	int length;
	DataType type;
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

#define UNPACKAGED_BUFFER_SIZE 150000
#define PACKAGED_BUFFER_SIZE 2000
//class MediaProcessor{
//	MediaProcessor();
//	virtual ~Mediaprocessor();
//private:
//	InputProcessor* input;
//	OutputProcessor* output;
//};

class RawDataReceiver {
public:
	virtual void receiveRawData(RawDataPacket& packet) = 0;
	virtual ~RawDataReceiver() {
	}
	;
};

class RTPDataReceiver {
public:
	virtual void receiveRtpData(unsigned char*rtpdata, int len) = 0;
	virtual ~RTPDataReceiver() {
	}
	;
};

class RTPSink;

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

	unsigned char* decodedAudioBuffer_;
	unsigned char* unpackagedAudioBuffer_;

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
	int init(const MediaInfo& info, RTPDataReceiver* rtpReceiver);

	void receiveRawData(RawDataPacket& packet);

private:

	int audioCoder;
	int videoCoder;

	int audioPackager;
	int videoPackager;

	unsigned int seqnum_;

	unsigned long timestamp_;

	unsigned char* encodedBuffer_;
	unsigned char* packagedBuffer_;
	unsigned char* rtpBuffer_;

	unsigned char* encodedAudioBuffer_;
	unsigned char* packagedAudioBuffer_;
	unsigned char* rtpAudioBuffer_;

	MediaInfo mediaInfo;

	RTPDataReceiver* rtpReceiver_;

	AVCodec* aCoder;
	AVCodecContext* aCoderContext;

	AVCodec* vCoder;
	AVCodecContext* vCoderContext;
	AVFrame* cPicture;

	AVFormatContext* aOutputFormatContext;
	AVOutputFormat* aOutputFormat;

	RTPInfo* vRTPInfo_;
	RTPSink* sink_;

	AVFormatContext* vOutputFormatContext;
	AVOutputFormat* vOutputFormat;

	RtpParser pars;

	bool initAudioCoder();
	bool initVideoCoder();

	bool initAudioPackager();
	bool initVideoPackager();

	int encodeAudio(unsigned char* inBuff, int nSamples,
			AVPacket* pkt);

	int encodeVideo(unsigned char* inBuff, int inBuffLen, AVPacket* pkt);

	int packageAudio(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff);

	int packageVideo(AVPacket* pkt, unsigned char* outBuff);
};
} /* namespace erizo */

#endif /* MEDIAPROCESSOR_H_ */
