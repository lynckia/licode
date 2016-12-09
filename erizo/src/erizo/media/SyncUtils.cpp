
#include "SyncUtils.h"
#include <sys/time.h>
#include <vector>
#include <cmath>
#include "../rtp/RtpHeaders.h"


namespace erizo {
  DEFINE_LOGGER(SyncUtils, "media.SyncUtils");
  const double kNtpFracPerMs = 4.294967296E6;

  // Constructor
  SyncUtils::SyncUtils(): firstVideoRtcpTimestamp(0), firstAudioRtcpTimestamp(0) {}

  // Converts an NTP timestamp to a millisecond timestamp.
  int64_t SyncUtils::NtpToMs(uint32_t ntp_secs, uint32_t ntp_frac) {
    const double ntp_frac_ms = static_cast<double>(ntp_frac) / kNtpFracPerMs;
    return ntp_secs * 1000 + ntp_frac_ms + 0.5;
  }

  // Utility function to avoid overflow (since rtpTimestamp is an uint32_t value)
  // when getting the rtpTimestamp of an rtcp packet
  long long SyncUtils::GetVectorPosition(int position, std::vector<RtcpHeader>& rtcp, bool isVideo) { //NOLINT
    long long toReturn = rtcp[position].getRtpTimestamp();  //NOLINT
    uint32_t firstRtpTimestamp = 0;
    if (isVideo) {
      firstRtpTimestamp = firstVideoRtcpTimestamp;
    } else {
      firstRtpTimestamp = firstAudioRtcpTimestamp;
    }
    if (firstRtpTimestamp != 0 && toReturn < firstRtpTimestamp) {
      toReturn += 0xFFFFFFFF;
      ELOG_DEBUG("Timestamp Wrapped - isVideo: %d, firstRTPTimestamp: %lld, wrapped: %lld",
      isVideo, firstRtpTimestamp, toReturn);
    }
    return toReturn;
  }

  // Given a vector of rtcp packets and a timestamp, it finds the corresponding rtcp packet.
  // It discard the old and unused rtcp from the vector and return true when finished or return false if the given rtp
  // timestamp is < of the first rtcp timestamp
  bool SyncUtils::Slide(long long rtp_timestamp, std::vector<RtcpHeader>& rtcp, bool isVideo) { //NOLINT
    // Save the first RTCP to track overflows
    if (isVideo && firstVideoRtcpTimestamp == 0) {
      firstVideoRtcpTimestamp = GetVectorPosition(0, rtcp, isVideo);
    } else if (!isVideo && firstAudioRtcpTimestamp == 0) {
      firstAudioRtcpTimestamp = GetVectorPosition(0, rtcp, isVideo);
    }
    // If the rtp timestamp is < of the first rtcp packet timestamp, return false.
    // (discard this packet to avoid problems)
    if (rtp_timestamp < GetVectorPosition(0, rtcp, isVideo)) {
      ELOG_WARN("RtpToNtpMs: isVideo: %d, This packet is too early. Discard to avoid problems %lld / %lld, next: %lld",
      isVideo, rtp_timestamp, GetVectorPosition(0, rtcp, isVideo), GetVectorPosition(1, rtcp, isVideo));
      return false;
    }
    // Discard all the old rtcp packets.
    while (rtcp.size() >1 && rtp_timestamp > GetVectorPosition(1, rtcp, isVideo)) {
      ELOG_DEBUG("RtpToNtpMs: isVideo: %d, Drop an old rtcp packet. rtp: %lld, next rtcp: %lld",
      isVideo, rtp_timestamp, GetVectorPosition(1, rtcp, isVideo));
      rtcp.erase(rtcp.begin());
    }
    return true;
  }

}  // namespace erizo
