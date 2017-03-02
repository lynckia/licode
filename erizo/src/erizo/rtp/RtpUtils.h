#ifndef ERIZO_SRC_ERIZO_RTP_RTPUTILS_H_
#define ERIZO_SRC_ERIZO_RTP_RTPUTILS_H_

#include "rtp/RtpHeaders.h"

#include "./MediaDefinitions.h"

#include <stdint.h>

#include <memory>

namespace erizo {

class RtpUtils {
 public:
  static bool sequenceNumberLessThan(uint16_t first, uint16_t second);

  static void forEachRRBlock(std::shared_ptr<dataPacket> packet, std::function<void(RtcpHeader*)> f);

  static void updateREMB(RtcpHeader *chead, uint bitrate);

  static void forEachNack(RtcpHeader *chead, std::function<void(uint16_t, uint16_t)> f);

  static std::shared_ptr<dataPacket> createPLI(uint32_t source_ssrc, uint32_t sink_ssrc);

  static int getPaddingLength(std::shared_ptr<dataPacket> packet);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPUTILS_H_
