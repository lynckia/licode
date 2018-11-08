#ifndef ERIZO_SRC_ERIZO_RTP_RTPVP8PARSER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPVP8PARSER_H_

#include "./logger.h"

namespace erizo {

enum VP8FrameTypes {
  kVP8IFrame,  // key frame
  kVP8PFrame   // Delta frame
};

typedef struct {
  bool nonReferenceFrame = false;
  bool beginningOfPartition = false;
  int partitionID = -1;
  bool hasPictureID = false;
  bool hasTl0PicIdx = false;
  bool hasTID = false;
  bool hasKeyIdx = false;
  int pictureID = -1;
  int tl0PicIdx = -1;
  int tID = -1;
  bool layerSync = false;
  int keyIdx = -1;
  int frameWidth = -1;
  int frameHeight = -1;
  VP8FrameTypes frameType = kVP8PFrame;

  const unsigned char* data;
  unsigned int dataLength;
} RTPPayloadVP8;

class RtpVP8Parser {
  DECLARE_LOGGER();
 public:
  RtpVP8Parser();
  virtual ~RtpVP8Parser();
  static void setVP8PictureID(unsigned char* data, int data_length, int picture_id);
  static void setVP8TL0PicIdx(unsigned char* data, int data_length, uint8_t tl0_pic_idx);
  static int removePictureID(unsigned char* data, int data_length);
  static int removeTl0PicIdx(unsigned char* data, int data_length);
  static int removeTIDAndKeyIdx(unsigned char* data, int data_length);
  erizo::RTPPayloadVP8* parseVP8(unsigned char* data, int datalength);
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_RTP_RTPVP8PARSER_H_
