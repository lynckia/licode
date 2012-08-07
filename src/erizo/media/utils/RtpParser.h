#ifndef RTPUTILS_H_
#define RTPUTILS_H_

namespace erizo {

typedef struct {
	bool nonReferenceFrame;
	bool beginningOfPartition;
	int partitionID;
	bool hasPictureID;
	bool hasTl0PicIdx;
	bool hasTID;
	bool hasKeyIdx;
	int pictureID;
	int tl0PicIdx;
	int tID;
	bool layerSync;
	int keyIdx;
	int frameWidth;
	int frameHeight;

	const unsigned char* data;
	unsigned int dataLength;
} RTPPayloadVP8;

enum FrameTypes {
	kIFrame, // key frame
	kPFrame // Delta frame
};

class RtpParser {

public:
	RtpParser();
	virtual ~RtpParser();
	erizo::RTPPayloadVP8* parseVP8(unsigned char* data, int datalength);
};

} /* namespace erizo */
#endif /* RTPUTILS_H_ */
