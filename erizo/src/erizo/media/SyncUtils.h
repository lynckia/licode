#ifndef SYNCUTILS_H_
#define SYNCUTILS_H_
#include "../rtp/RtpHeaders.h"
#include "logger.h"
#include <vector>

namespace erizo {

    // our structure which will store every rtcp packet
    struct Measurements {
            Measurements() : rtcp_Packets(){}
            std::vector<RtcpHeader> rtcp_Packets;
    };

    class SyncUtils {
        DECLARE_LOGGER();
        public:
        // Constructor
        SyncUtils();

        int64_t NtpToMs(uint32_t ntp_secs, uint32_t ntp_frac);
        long long GetVectorPosition(int position, std::vector<RtcpHeader>& rtcp, bool isVideo);
        bool Slide(long long rtp_timestamp, std::vector<RtcpHeader>& rtcp, bool isVideo);

        private:
        uint32_t firstVideoRtcpTimestamp, firstAudioRtcpTimestamp;
    };
}
#endif
