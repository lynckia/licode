/*
 * Stats.h
 */
#ifndef ERIZO_SRC_ERIZO_STATS_H_
#define ERIZO_SRC_ERIZO_STATS_H_

#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include <string>
#include <map>

#include "./logger.h"
#include "rtp/RtpHeaders.h"

namespace erizo {

class WebRtcConnectionStatsListener;

class Stats {
  DECLARE_LOGGER();

 public:
  Stats();
  virtual ~Stats();

  uint32_t processRtpPacket(char* buf, int len);
  void processRtcpPacket(char* buf, int length);
  std::string getStats();
  // The video and audio SSRCs of the Client
  void setVideoSourceSSRC(unsigned int ssrc) {
    videoSSRC_ = ssrc;
  }
  void setAudioSourceSSRC(unsigned int ssrc) {
    audioSSRC_ = ssrc;
  }
  inline void setStatsListener(WebRtcConnectionStatsListener* listener) {
    this->theListener_ = listener;
  }

 private:
  uint64_t rtpBytesReceived_, packetsReceived_;
  struct timeval bitRateCalculationStart_;

  typedef std::map<std::string, uint64_t> singleSSRCstatsMap_t;
  typedef std::map <uint32_t, singleSSRCstatsMap_t> fullStatsMap_t;
  fullStatsMap_t statsPacket_;
  boost::recursive_mutex mapMutex_;
  WebRtcConnectionStatsListener* theListener_;
  unsigned int videoSSRC_, audioSSRC_;

  void processRtcpPacket(RtcpHeader* chead);

  uint32_t getPacketsReceived(unsigned int ssrc) {
    return(statsPacket_[ssrc]["packetsReceived"]);
  }

  uint32_t getBitRateReceived(unsigned int ssrc) {
    return(statsPacket_[ssrc]["bitRateReceived"]);
  }

  uint32_t getPacketsLost(unsigned int ssrc) {
    return (statsPacket_[ssrc]["packetsLost"]);
  }
  void setPacketsLost(uint32_t packets, unsigned int ssrc) {
    statsPacket_[ssrc]["packetsLost"] = packets;
  }

  uint8_t getFractionLost(unsigned int ssrc) {
    return statsPacket_[ssrc]["fractionLost"];
  }
  void setFractionLost(uint8_t fragment, unsigned int ssrc) {
    statsPacket_[ssrc]["fractionLost"] = fragment;
  }

  uint32_t getRtcpPacketSent(unsigned int ssrc) {
    return statsPacket_[ssrc]["rtcpPacketSent"];
  }
  void setRtcpPacketSent(uint32_t count, unsigned int ssrc) {
    statsPacket_[ssrc]["rtcpPacketSent"] = count;
  }

  uint32_t getRtcpBytesSent(unsigned int ssrc) {
    return statsPacket_[ssrc]["rtcpBytesSent"];
  }
  void setRtcpBytesSent(unsigned int count, unsigned int ssrc) {
    statsPacket_[ssrc]["rtcpBytesSent"] = count;
  }
  uint32_t getJitter(unsigned int ssrc) {
    return statsPacket_[ssrc]["jitter"];
  }
  void setJitter(uint32_t count, unsigned int ssrc) {
    statsPacket_[ssrc]["jitter"] = count;
  }

  uint32_t getBandwidth(unsigned int ssrc) {
    return statsPacket_[ssrc]["bandwidth"];
  }
  void setBandwidth(uint64_t count, unsigned int ssrc) {
    statsPacket_[ssrc]["bandwidth"] = count;
  }

  void accountPLIMessage(unsigned int ssrc) {
    if (statsPacket_[ssrc].count("PLI")) {
      statsPacket_[ssrc]["PLI"]++;
    } else {
      statsPacket_[ssrc]["PLI"] = 1;
    }
  }
  void accountSLIMessage(unsigned int ssrc) {
    if (statsPacket_[ssrc].count("SLI")) {
      statsPacket_[ssrc]["SLI"]++;
    } else {
      statsPacket_[ssrc]["SLI"] = 1;
    }
  }
  void accountFIRMessage(unsigned int ssrc) {
    if (statsPacket_[ssrc].count("FIR")) {
      statsPacket_[ssrc]["FIR"]++;
    } else {
      statsPacket_[ssrc]["FIR"] = 1;
    }
  }

  void accountNACKMessage(unsigned int ssrc) {
    if (statsPacket_[ssrc].count("NACK")) {
      statsPacket_[ssrc]["NACK"]++;
    } else {
      statsPacket_[ssrc]["NACK"] = 1;
    }
  }

  void accountRTPPacket(unsigned int ssrc) {
    if (statsPacket_[ssrc].count("receivedRTPPackets")) {
      statsPacket_[ssrc]["receivedRTPPackets"]++;
    } else {
      statsPacket_[ssrc]["receivedRTPPackets"] = 1;
    }
  }

  unsigned int getSourceSSRC(unsigned int sourceSSRC, unsigned int ssrc) {
    return statsPacket_[ssrc]["sourceSsrc"];
  }
  void setSourceSSRC(unsigned int sourceSSRC, unsigned int ssrc) {
    statsPacket_[ssrc]["sourceSsrc"] = sourceSSRC;
  }

  void sendStats();
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_STATS_H_
