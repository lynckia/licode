#ifndef ERIZO_SRC_ERIZO_RTP_RTPVP8PARSER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPVP8PARSER_H_

#include "./logger.h"

namespace erizo {

enum FrameTypes {
  kIFrame,  // key frame
  kPFrame   // Delta frame
};

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
  FrameTypes frameType;

  const unsigned char* data;
  unsigned int dataLength;
} RTPPayloadVP8;

class RtpVP8Parser {
  DECLARE_LOGGER();
 public:
  RtpVP8Parser();
  virtual ~RtpVP8Parser();
  erizo::RTPPayloadVP8* parseVP8(unsigned char* data, int datalength);
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_RTP_RTPVP8PARSER_H_
