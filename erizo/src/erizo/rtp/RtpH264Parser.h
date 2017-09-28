#ifndef ERIZO_SRC_ERIZO_RTP_RTPH264PARSER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPH264PARSER_H_

#include <vector>

#include "./logger.h"

namespace erizo {

enum H264FrameTypes {
  kH264IFrame,  // key frame
  kH264PFrame   // Delta frame
};

typedef struct {
  const unsigned char* data;
  unsigned int dataLength;
  H264FrameTypes frameType = kH264PFrame;
} RTPPayloadH264;

class RtpH264Parser {
  DECLARE_LOGGER();
 public:
  RtpH264Parser();
  virtual ~RtpH264Parser();
  erizo::RTPPayloadH264* parseH264(unsigned char* data, int datalength);
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_RTP_RTPH264PARSER_H_
