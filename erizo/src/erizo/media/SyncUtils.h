#ifndef ERIZO_SRC_ERIZO_MEDIA_SYNCUTILS_H_
#define ERIZO_SRC_ERIZO_MEDIA_SYNCUTILS_H_
#include <vector>
#include "../rtp/RtpHeaders.h"
#include "./logger.h"

namespace erizo {

// our structure which will store every rtcp packet
struct Measurements {
    Measurements() : rtcp_Packets() {}
    std::vector<RtcpHeader> rtcp_Packets;
};

class SyncUtils {
    DECLARE_LOGGER();
 public:
    // Constructor
    SyncUtils();

    int64_t NtpToMs(uint32_t ntp_secs, uint32_t ntp_frac);
    long long GetVectorPosition(int position, std::vector<RtcpHeader>& rtcp, bool isVideo); //NOLINT
    bool Slide(long long rtp_timestamp, std::vector<RtcpHeader>& rtcp, bool isVideo); //NOLINT

 private:
     uint32_t firstVideoRtcpTimestamp, firstAudioRtcpTimestamp;
};
}  // namespace erizo
#endif // ERIZO_SRC_ERIZO_MEDIA_SYNCUTILS_H_
