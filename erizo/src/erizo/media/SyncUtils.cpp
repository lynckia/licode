
#include "SyncUtils.h"
#include "../rtp/RtpHeaders.h"
#include <vector>


namespace erizo {
  DEFINE_LOGGER(SyncUtils, "media.SyncUtils");
  const double kNtpFracPerMs = 4.294967296E6;

  // Constructor
  SyncUtils::SyncUtils(){};

  // Converts an NTP timestamp to a millisecond timestamp.
  int64_t SyncUtils::NtpToMs(uint32_t ntp_secs, uint32_t ntp_frac) {
          const double ntp_frac_ms = static_cast<double>(ntp_frac) / kNtpFracPerMs;
          return ntp_secs * 1000 + ntp_frac_ms + 0.5;
  }

  // Converts |rtp_timestamp| to the NTP time base using the NTP and RTP timestamp
  // pairs in |rtcp|. The converted timestamp is returned in
  // |rtp_timestamp_in_ms|. This function compensates for wrap arounds in RTP
  // timestamps and returns false if it can't do the conversion due to reordering
  //
  // !! ASSUMING rtcp vector has at least 2 rtcp packets
  bool SyncUtils::RtpToNtpMs(uint32_t rtp_timestamp, std::vector<RtcpHeader>& rtcp, int64_t* rtp_timestamp_in_ms) {
          if (rtcp.size() < 2) {
                  ELOG_WARN("RtpToNtpMs: invoked with too few rtcp packets");
                  *rtp_timestamp_in_ms = 0;
                  return false;
          }
          if (rtp_timestamp < rtcp[0].getRtpTimestamp()) {
                  ELOG_WARN("RtpToNtpMs: This packet is too early. Discard to avoid problems");
                  *rtp_timestamp_in_ms = 0;
                  return false;
          }
          while(rtp_timestamp > rtcp[1].getRtpTimestamp() && rtcp.size() > 2) {
                  ELOG_DEBUG("RtpToNtpMs: Drop an old rtcp packet");
                  rtcp.erase(rtcp.begin());
          }

          //TODO
          // assert(rtcp.size() == 2);
          int64_t rtcp_ntp_ms_new = SyncUtils::NtpToMs(rtcp[1].getNtpTimestampMSW(), rtcp[1].getNtpTimestampLSW());
          int64_t rtcp_ntp_ms_old = SyncUtils::NtpToMs(rtcp[0].getNtpTimestampMSW(), rtcp[0].getNtpTimestampLSW());
          int64_t rtcp_timestamp_new = rtcp[1].getRtpTimestamp();
          int64_t rtcp_timestamp_old = rtcp[0].getRtpTimestamp();

          if (!CompensateForWrapAround(rtcp_timestamp_new, rtcp_timestamp_old, &rtcp_timestamp_new)) {
                  return false;
          }
          double freq_khz;
          if (!CalculateFrequency(rtcp_ntp_ms_new, rtcp_timestamp_new, rtcp_ntp_ms_old, rtcp_timestamp_old, &freq_khz)) {
                  return false;
          }
          double offset = rtcp_timestamp_new - freq_khz * rtcp_ntp_ms_new;
          int64_t rtp_timestamp_unwrapped;
          if (!CompensateForWrapAround(rtp_timestamp, rtcp_timestamp_old, &rtp_timestamp_unwrapped)) {
                  return false;
          }
          double rtp_timestamp_ntp_ms = (static_cast<double>(rtp_timestamp_unwrapped) - offset) / freq_khz + 0.5f;
          if (rtp_timestamp_ntp_ms < 0) {
                  return false;
          }
          *rtp_timestamp_in_ms = rtp_timestamp_ntp_ms;
          return true;
  }

  bool SyncUtils::CompensateForWrapAround(uint32_t new_timestamp, uint32_t old_timestamp, int64_t* compensated_timestamp) {
          //TODO
          // assert(compensated_timestamp);
          int64_t wraps = SyncUtils::CheckForWrapArounds(new_timestamp, old_timestamp);
          if (wraps < 0) {
                  // Reordering, don't use this packet.
                  return false;
          }
          *compensated_timestamp = new_timestamp + (wraps << 32);
          return true;
  }

  int SyncUtils::CheckForWrapArounds(uint32_t new_timestamp, uint32_t old_timestamp) {
          if (new_timestamp < old_timestamp) {
                  // This difference should be less than -2^31 if we have had a wrap around
                  // (e.g. |new_timestamp| = 1, |rtcp_rtp_timestamp| = 2^32 - 1). Since it is
                  // cast to a int32_t, it should be positive.
                  if (static_cast<int32_t>(new_timestamp - old_timestamp) > 0) {
                          // Forward wrap around.
                          return 1;
                  }
          } else if (static_cast<int32_t>(old_timestamp - new_timestamp) > 0) {
                  // This difference should be less than -2^31 if we have had a backward wrap
                  // around. Since it is cast to a int32_t, it should be positive.
                  return -1;
          }
          return 0;
  }

  // Calculates the RTP timestamp frequency from two pairs of NTP and RTP
  // timestamps.
  bool SyncUtils::CalculateFrequency(int64_t rtcp_ntp_ms1, uint32_t rtp_timestamp1, int64_t rtcp_ntp_ms2, uint32_t rtp_timestamp2, double* frequency_khz) {
          if (rtcp_ntp_ms1 <= rtcp_ntp_ms2) {
                  return false;
          }
          *frequency_khz = static_cast<double>(rtp_timestamp1 - rtp_timestamp2) /
                           static_cast<double>(rtcp_ntp_ms1 - rtcp_ntp_ms2);
          return true;
  }
}
