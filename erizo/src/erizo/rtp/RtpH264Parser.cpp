#include "rtp/RtpH264Parser.h"

#include <arpa/inet.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <iostream>

namespace erizo {

DEFINE_LOGGER(RtpH264Parser, "rtp.RtpH264Parser");

RtpH264Parser::RtpH264Parser() {
}

RtpH264Parser::~RtpH264Parser() {
}

//
// H264 format:
//
// +---------------+
// |0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+
// |F|NRI|  Type   |
// +---------------+

RTPPayloadH264* RtpH264Parser::parseH264(unsigned char* data, int dataLength) {
  // ELOG_DEBUG("Parsing H264 %d bytes", dataLength);
  RTPPayloadH264* h264 = new RTPPayloadH264;
  const unsigned char* dataPtr = data;
  int fragment_type = *dataPtr & 0x1F;
  dataPtr++;
  int nal_type = *dataPtr & 0x1F;
  int start_bit = *dataPtr & 0x80;
  bool is_keyframe = ((fragment_type == 28 || fragment_type == 29)
                     && nal_type == 5 && start_bit == 128) || fragment_type == 5;
  h264->frameType = is_keyframe ? kH264IFrame : kH264PFrame;
  dataPtr++;

  h264->data = dataPtr;
  h264->dataLength = (unsigned int) dataLength;

  return h264;
}

}  // namespace erizo
