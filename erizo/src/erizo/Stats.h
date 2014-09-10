/*
 * Stats.h
 */

#ifndef STATS_H_
#define STATS_H_

#include <string>
#include <map>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include "logger.h"
#include "rtp/RtpHeaders.h"

namespace erizo{

  class WebRtcConnectionStatsListener;

  class Stats{
    DECLARE_LOGGER();
    public:
    Stats(unsigned long int ssrc=0):SSRC_(ssrc){
      currentIterations_ = 0;
      runningStats_ = false;
      fragmentLostValues_ = 0;
    };

    virtual ~Stats();

    void processRtcpStats(char* buf, int length);
    std::string getStats();
    void setPeriodicStats(int intervalMillis, WebRtcConnectionStatsListener* listener);


    private:
    typedef std::map<std::string, unsigned long int> singleSSRCstatsMap_t;
    typedef std::map <unsigned long int, singleSSRCstatsMap_t> fullStatsMap_t;
    fullStatsMap_t theStats_;
    static const int SLEEP_INTERVAL_ = 100000;
    unsigned int SSRC_;
    unsigned int fragmentLostValues_;      
    boost::mutex mapMutex_;
    WebRtcConnectionStatsListener* theListener_;      
    boost::thread statsThread_;
    int iterationsPerTick_;
    int currentIterations_;
    bool runningStats_;

    void processRtcpStats(RtcpHeader* chead);

    int getPacketsLost(unsigned int ssrc){
      return static_cast<int>(theStats_[ssrc]["packetsLost"]);
    };
    void setPacketsLost(int packets, unsigned int ssrc){
      theStats_[ssrc]["packetsLost"] = static_cast<unsigned int>(packets);
    };

    unsigned int getFragmentLost(unsigned int ssrc){
      return theStats_[ssrc]["packetsLost"];
    };
    void addFragmentLost(unsigned int fragment, unsigned int SSRC){
      theStats_[SSRC]["fragmentLost"] += fragment;
    };

    unsigned int getRtcpPacketSent(unsigned int ssrc){
      return theStats_[ssrc]["rtcpPacketSent"];
    };
    void setRtcpPacketSent(unsigned int count, unsigned int ssrc){
      theStats_[ssrc]["rtcpPacketSent"] = count;
    };

    unsigned int getRtcpBytesSent(unsigned int ssrc){
      return theStats_[ssrc]["rtcpBytesSent"];
    };
    void setRtcpBytesSent(unsigned int count, unsigned int ssrc){
      theStats_[ssrc]["rtcpBytesSent"] = count;
    };
    unsigned int getJitter(unsigned int ssrc){
      return theStats_[ssrc]["jitter"];
    };
    void setJitter(unsigned int count, unsigned int ssrc){
      theStats_[ssrc]["jitter"] = count;
    };

    void sendStats();
  };

}

#endif /* STATS_H_ */

