#include "rtp/RtpUtils.h"


namespace erizo {

bool RtpUtils::sequenceNumberLessThan(uint16_t first, uint16_t last) {
  uint16_t result = first - last;
  return result > 0xF000;
}

}
