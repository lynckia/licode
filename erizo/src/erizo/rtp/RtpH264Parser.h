#ifndef ERIZO_SRC_ERIZO_RTP_RTPH264PARSER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPH264PARSER_H_

#include "./logger.h"
#include <memory>

namespace erizo {

enum H264FrameTypes {
  kH264IFrame,  // key frame
  kH264PFrame   // Delta frame
};

enum NALTypes {
    single,
    fragmented,
    aggregated,
};

struct RTPPayloadH264 {
  static constexpr unsigned char start_sequence[] = { 0, 0, 0, 1 };
  H264FrameTypes frameType = kH264PFrame;
  NALTypes nal_type = single;
  const unsigned char* data;
  unsigned int dataLength = 0;
  unsigned char fragment_nal_header;
  int fragment_nal_header_len = 0;
  unsigned char start_bit = 0;
  unsigned char end_bit = 0;
  std::unique_ptr<unsigned char[]> unpacked_data;
  unsigned unpacked_data_len = 0;
};

class RtpH264Parser {
  DECLARE_LOGGER();
 public:
  RtpH264Parser();
  virtual ~RtpH264Parser();
  erizo::RTPPayloadH264* parseH264(unsigned char* data, int datalength);
 private:
  int parse_packet_fu_a(RTPPayloadH264* h264, unsigned char* buf, int len) const;
  int parse_aggregated_packet(RTPPayloadH264* h264, unsigned char* buf, int len) const;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_RTP_RTPH264PARSER_H_
