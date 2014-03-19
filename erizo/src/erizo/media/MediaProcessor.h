#ifndef MEDIAPROCESSOR_H_
#define MEDIAPROCESSOR_H_

#include <boost/cstdint.hpp>
#include <sys/time.h>
#include <arpa/inet.h>
#include <string>

#include "rtp/RtpVP8Parser.h"
#include "../MediaDefinitions.h"
#include "codecs/Codecs.h"
#include "codecs/VideoCodec.h"
#include "logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

}

namespace erizo {


struct RTPInfo {
	enum AVCodecID codec;
	unsigned int ssrc;
	unsigned int PT;
};

enum ProcessorType {
	RTP_ONLY, AVF, PACKAGE_ONLY
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
	ProcessorType processorType;
	RTPInfo rtpVideoInfo;
	RTPInfo rtpAudioInfo;
	VideoCodecInfo videoCodec;
	AudioCodecInfo audioCodec;

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
	virtual void receiveRtpData(unsigned char* rtpdata, int len) = 0;
	virtual ~RTPDataReceiver() {
	}
	;
};

class RTPSink;

class InputProcessor: MediaSink {
	DECLARE_LOGGER();
public:
	InputProcessor();
	virtual ~InputProcessor();

	int init(const MediaInfo& info, RawDataReceiver* receiver);

	int deliverAudioData(char* buf, int len);
	int deliverVideoData(char* buf, int len);

	int unpackageVideo(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff, int* gotFrame, int* estimatedFps);

	int unpackageAudio(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff);

  void closeSink();
  void close();

private:

	int audioDecoder;
	int videoDecoder;

  double lastVideoTs_;

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


	AVFormatContext* aInputFormatContext;
	AVInputFormat* aInputFormat;
  VideoDecoder vDecoder;

	RTPInfo* vRTPInfo;

	AVFormatContext* vInputFormatContext;
	AVInputFormat* vInputFormat;

	RawDataReceiver* rawReceiver_;

	erizo::RtpVP8Parser pars;

	bool initAudioDecoder();

	bool initAudioUnpackager();
	bool initVideoUnpackager();

	int decodeAudio(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff);

};
class OutputProcessor: public RawDataReceiver {
	DECLARE_LOGGER();
public:

	OutputProcessor();
	virtual ~OutputProcessor();
	int init(const MediaInfo& info, RTPDataReceiver* rtpReceiver);
  void close();
	void receiveRawData(RawDataPacket& packet);

  int packageAudio(unsigned char* inBuff, int inBuffLen,
			unsigned char* outBuff);

	int packageVideo(unsigned char* inBuff, int buffSize, unsigned char* outBuff);

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

  VideoEncoder vCoder;


	AVFormatContext* aOutputFormatContext;
	AVOutputFormat* aOutputFormat;

	RTPInfo* vRTPInfo_;
	RTPSink* sink_;

	AVFormatContext* vOutputFormatContext;
	AVOutputFormat* vOutputFormat;

	RtpVP8Parser pars;

	bool initAudioCoder();

	bool initAudioPackager();
	bool initVideoPackager();

	int encodeAudio(unsigned char* inBuff, int nSamples,
			AVPacket* pkt);

};
} /* namespace erizo */

#endif /* MEDIAPROCESSOR_H_ */
