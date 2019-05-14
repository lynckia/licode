#include "rtp/RtpH264Parser.h"

#include <arpa/inet.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <iostream>
#include <cstdint>
#include <cstring>
#include "libavutil/intreadwrite.h"

namespace erizo {

constexpr unsigned char RTPPayloadH264::start_sequence[];

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

RTPPayloadH264* RtpH264Parser::parseH264(unsigned char* buf, int len) {
  RTPPayloadH264* h264 = new RTPPayloadH264;
  uint8_t nal;
  uint8_t type;

  if (!len) {
    ELOG_ERROR("Empty H.264 RTP packet");
    return h264;
  }

  nal  = buf[0];
  type = nal & 0x1f;

  /* Simplify the case (these are all the NAL types used internally by the H.264 codec). */
  if (type >= 1 && type <= 23 && type != 5) {
    type = 1;
  }

  switch (type) {
    case 5:             // keyframe
      h264->frameType = kH264IFrame;
      // Falls through
    case 0:          // undefined, but pass them through
    case 1:
      h264->nal_type = single;
      h264->data = buf;
      h264->dataLength = len;
      break;
    case 24:           // STAP-A (one packet, multiple nals)
      // Consume the STAP-A NAL
      buf++;
      len--;
      parse_aggregated_packet(h264, buf, len);
      break;
    case 25:           // STAP-B
    case 26:           // MTAP-16
    case 27:           // MTAP-24
    case 29:           // FU-B
      ELOG_ERROR("Unhandled H264 NAL unit type (%d)", type);
      break;
    case 28:           // FU-A (fragmented nal)
      parse_packet_fu_a(h264, buf, len);
      break;
    case 30:           // undefined
    case 31:           // undefined
    default:
      ELOG_ERROR("Undefined H264 NAL unit type (%d)", type);
      break;
  }

  return h264;
}

int RtpH264Parser::parse_packet_fu_a(RTPPayloadH264* h264, unsigned char* buf, int len) const {
  uint8_t fu_indicator, fu_header, start_bit, nal_type, nal, end_bit;

  if (len < 3) {
    ELOG_ERROR("Too short data for FU-A H.264 RTP packet");
    return -1;
  }

  fu_indicator = buf[0];
  fu_header  = buf[1];
  start_bit  = fu_header >> 7;
  end_bit    = fu_header & 0x40;
  nal_type   = fu_header & 0x1f;
  nal      = (fu_indicator & 0xe0) | nal_type;

  // skip the fu_indicator and fu_header
  buf += 2;
  len -= 2;

  if (nal_type == 5 && start_bit == 1) {
    h264->frameType = kH264IFrame;
  }

  h264->nal_type = fragmented;
  h264->start_bit = start_bit;
  h264->end_bit = end_bit;
  h264->fragment_nal_header = nal;
  h264->fragment_nal_header_len = 1;
  h264->data = buf;
  h264->dataLength = len;
  return 0;
}

int RtpH264Parser::parse_aggregated_packet(RTPPayloadH264* h264, unsigned char* buf, int len) const {
  h264->nal_type = aggregated;
  unsigned char* dst = nullptr;
  int pass     = 0;
  int total_length = 0;

  // first we are going to figure out the total size
  for (pass = 0; pass < 2; pass++) {
    const uint8_t *src = buf;
    int src_len    = len;

    while (src_len > 2) {
      uint16_t nal_size = AV_RB16(src);

      // consume the length of the aggregate
      src   += 2;
      src_len -= 2;

      if (nal_size <= src_len) {
        if (pass == 0) {
          // counting
          total_length += sizeof(RTPPayloadH264::start_sequence) + nal_size;
        } else {
          // copying
          std::memcpy(dst, RTPPayloadH264::start_sequence, sizeof(RTPPayloadH264::start_sequence));
          dst += sizeof(RTPPayloadH264::start_sequence);
          std::memcpy(dst, src, nal_size);
          dst += nal_size;
          uint8_t nal_type = src[0] & 0x1f;
          if (nal_type == 5) {
            h264->frameType = kH264IFrame;
          }
        }
      } else {
        ELOG_ERROR("NAL size exceeds length: %d %d\n", nal_size, src_len);
        return -1;
      }

      // eat what we handled
      src   += nal_size;
      src_len -= nal_size;
    }

    if (pass == 0) {
      /* now we know the total size of the packet (with the start sequences added) */
      h264->unpacked_data_len = total_length;
      h264->unpacked_data = std::unique_ptr<unsigned char[]>(new unsigned char[total_length]);
      dst = &h264->unpacked_data[0];
    }
  }
  return 0;
}

}  // namespace erizo
