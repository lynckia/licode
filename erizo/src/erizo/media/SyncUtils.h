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

        // Converts an RTP timestamp to the NTP domain in milliseconds using two (RTP timestamp, NTP timestamp) pairs.
        bool RtpToNtpMs(uint32_t rtp_timestamp, std::vector<RtcpHeader>& rtcp, int64_t* timestamp_in_ms);
        bool CompensateForWrapAround(uint32_t new_timestamp, uint32_t old_timestamp, int64_t* compensated_timestamp);
        int CheckForWrapArounds(uint32_t new_timestamp, uint32_t old_timestamp);
        bool CalculateFrequency(int64_t rtcp_ntp_ms1, uint32_t rtp_timestamp1, int64_t rtcp_ntp_ms2, uint32_t rtp_timestamp2, double* frequency_khz);
        int64_t NtpToMs(uint32_t ntp_secs, uint32_t ntp_frac);
    };
}
#endif
