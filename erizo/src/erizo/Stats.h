/*
 * Stats.h
 */
#ifndef ERIZO_SRC_ERIZO_STATS_H_
#define ERIZO_SRC_ERIZO_STATS_H_

#include <atomic>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include <string>
#include <map>

#include "./logger.h"
#include "rtp/RtpHeaders.h"
#include "lib/Clock.h"

namespace erizo {

  constexpr duration kBitrateStatsPeriod = std::chrono::seconds(2);

class WebRtcConnectionStatsListener;

class Stats {
  DECLARE_LOGGER();

 public:
  Stats();
  virtual ~Stats();

  void processRtpPacket(char* buf, int len);
  void processRtcpPacket(char* buf, int length);
  void setEstimatedBandwidth(uint32_t bandwidth, uint32_t ssrc);
  void setSlideShowMode(bool is_active, uint32_t ssrc);
  void setMute(bool is_active, uint32_t ssrc);


  std::string getStats();
  // The video and audio SSRCs of the Client
  void setVideoSourceSSRC(unsigned int ssrc);
  void setAudioSourceSSRC(unsigned int ssrc);
  void setVideoSinkSSRC(unsigned int ssrc);
  void setAudioSinkSSRC(unsigned int ssrc);

  uint32_t getLatestTotalBitrate() const {
    return latest_total_bitrate_;
  }

  void setLatestTotalBitrate(uint32_t bitrate) {  // For testing purposes
    latest_total_bitrate_ = bitrate;
  }

  inline void setStatsListener(WebRtcConnectionStatsListener* listener) {
    this->theListener_ = listener;
  }

 private:
  time_point bitrate_calculation_start_;
  typedef std::map<std::string, uint64_t> singleSSRCstatsMap_t;
  typedef std::map <uint32_t, singleSSRCstatsMap_t> fullStatsMap_t;


  fullStatsMap_t ssrc_stats_;
  singleSSRCstatsMap_t stream_status_;

  std::map<uint32_t, uint32_t> bitrate_bytes_map;
  boost::recursive_mutex mapMutex_;
  WebRtcConnectionStatsListener* theListener_;
  std::atomic<uint32_t> video_source_ssrc_, video_sink_ssrc_;
  std::atomic<uint32_t> audio_source_ssrc_, audio_sink_ssrc_;

  uint32_t latest_total_bitrate_;

  uint32_t getPacketsReceived(unsigned int ssrc) {
    return(ssrc_stats_[ssrc]["packetsReceived"]);
  }
  uint32_t getBitrateCalculated(unsigned int ssrc) {
    return(ssrc_stats_[ssrc]["bitrateCalculated"]);
  }
  void setBitrateCalculated(uint32_t bitrate, uint32_t ssrc) {
    ssrc_stats_[ssrc]["bitrateCalculated"] = bitrate;
  }
  uint32_t getPacketsLost(unsigned int ssrc) {
    return (ssrc_stats_[ssrc]["packetsLost"]);
  }
  void setPacketsLost(uint32_t packets, unsigned int ssrc) {
    ssrc_stats_[ssrc]["packetsLost"] = packets;
  }

  uint8_t getFractionLost(unsigned int ssrc) {
    return ssrc_stats_[ssrc]["fractionLost"];
  }
  void setFractionLost(uint8_t fragment, unsigned int ssrc) {
    ssrc_stats_[ssrc]["fractionLost"] = fragment;
  }

  uint32_t getRtcpPacketSent(unsigned int ssrc) {
    return ssrc_stats_[ssrc]["rtcpPacketSent"];
  }
  void setRtcpPacketSent(uint32_t count, unsigned int ssrc) {
    ssrc_stats_[ssrc]["rtcpPacketSent"] = count;
  }

  uint32_t getRtcpBytesSent(unsigned int ssrc) {
    return ssrc_stats_[ssrc]["rtcpBytesSent"];
  }
  void setRtcpBytesSent(unsigned int count, unsigned int ssrc) {
    ssrc_stats_[ssrc]["rtcpBytesSent"] = count;
  }
  uint32_t getJitter(unsigned int ssrc) {
    return ssrc_stats_[ssrc]["jitter"];
  }
  void setJitter(uint32_t count, unsigned int ssrc) {
    ssrc_stats_[ssrc]["jitter"] = count;
  }

  uint32_t getBandwidth(unsigned int ssrc) {
    return ssrc_stats_[ssrc]["bandwidth"];
  }
  void setBandwidth(uint64_t count, unsigned int ssrc) {
    ssrc_stats_[ssrc]["bandwidth"] = count;
  }

  uint32_t getErizoEstimatedBandwidth(unsigned int ssrc) {
    return ssrc_stats_[ssrc]["erizoBandwidth"];
  }
  void setErizoEstimatedBandwidth(uint32_t count, unsigned int ssrc) {
    ssrc_stats_[ssrc]["erizoBandwidth"] = count;
  }

  uint32_t getErizoSlideShow(unsigned int ssrc) {
    return ssrc_stats_[ssrc]["erizoSlideShow"];
  }
  void setErizoSlideShow(uint32_t is_active, unsigned int ssrc) {
    ssrc_stats_[ssrc]["erizoSlideShow"] = is_active;
  }

  uint32_t getErizoMute(unsigned int ssrc) {
    return ssrc_stats_[ssrc]["erizoMute"];
  }
  void setErizoMute(uint32_t is_active, unsigned int ssrc) {
    ssrc_stats_[ssrc]["erizoMute"] = is_active;
  }

  void accountPLIMessage(unsigned int ssrc) {
    if (ssrc_stats_[ssrc].count("PLI")) {
      ssrc_stats_[ssrc]["PLI"]++;
    } else {
      ssrc_stats_[ssrc]["PLI"] = 1;
    }
  }
  void accountSLIMessage(unsigned int ssrc) {
    if (ssrc_stats_[ssrc].count("SLI")) {
      ssrc_stats_[ssrc]["SLI"]++;
    } else {
      ssrc_stats_[ssrc]["SLI"] = 1;
    }
  }
  void accountFIRMessage(unsigned int ssrc) {
    if (ssrc_stats_[ssrc].count("FIR")) {
      ssrc_stats_[ssrc]["FIR"]++;
    } else {
      ssrc_stats_[ssrc]["FIR"] = 1;
    }
  }

  void accountNACKMessage(unsigned int ssrc) {
    if (ssrc_stats_[ssrc].count("NACK")) {
      ssrc_stats_[ssrc]["NACK"]++;
    } else {
      ssrc_stats_[ssrc]["NACK"] = 1;
    }
  }

  void accountRTPPacket(unsigned int ssrc) {
    if (ssrc_stats_[ssrc].count("receivedRTPPackets")) {
      ssrc_stats_[ssrc]["receivedRTPPackets"]++;
    } else {
      ssrc_stats_[ssrc]["receivedRTPPackets"] = 1;
    }
  }

  unsigned int getSourceSSRC(unsigned int sourceSSRC, unsigned int ssrc) {
    return ssrc_stats_[ssrc]["sourceSsrc"];
  }
  void setSourceSSRC(unsigned int sourceSSRC, unsigned int ssrc) {
    ssrc_stats_[ssrc]["sourceSsrc"] = sourceSSRC;
  }

  bool isSourceSSRC(uint32_t ssrc) {
    return (ssrc == video_source_ssrc_ || ssrc == audio_source_ssrc_);
  }

  bool isSinkSSRC(uint32_t ssrc) {
    return (ssrc == video_sink_ssrc_ || ssrc == audio_sink_ssrc_);
  }

  void sendStats();
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_STATS_H_
