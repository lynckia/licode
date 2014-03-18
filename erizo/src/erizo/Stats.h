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
    };

    virtual ~Stats();

    void processRtcpStats(RtcpHeader* chead);
    std::string getStats();
    void setPeriodicStats(int intervalMillis, WebRtcConnectionStatsListener* listener);


    private:
    std::map <std::string, unsigned int> theStats_;
    unsigned long int SSRC_;
    unsigned int fragmentLostValues_;      
    boost::mutex mapMutex_;
    WebRtcConnectionStatsListener* theListener_;      
    boost::thread statsThread_;
    int interval_;
    bool runningStats_;

    int getPacketsLost(){
      return static_cast<int>(theStats_["packetsLost"]);
    };
    void setPacketsLost(int packets){
      theStats_["packetsLost"] = static_cast<unsigned int>(packets);
    };

    unsigned int getFragmentLost(){
      return theStats_["packetsLost"];
    };
    void addFragmentLost(unsigned int fragment){
      theStats_["fragmentLost"] += fragment;
    };

    unsigned int getRtcpPacketSent(){
      return theStats_["rtcpPacketSent"];
    };
    void setRtcpPacketSent(unsigned int count){
      theStats_["rtcpPacketSent"] = count;
    };

    unsigned int getRtcpBytesSent(){
      return theStats_["rtcpBytesSent"];
    };
    void setRtcpBytesSent(unsigned int count){
      theStats_["rtcpBytesSent"] = count;
    };
    unsigned int getJitter(){
      return theStats_["jitter"];
    };
    void setJitter(unsigned int count){
      theStats_["jitter"] = count;
    };

    void sendStats();
  };

}

#endif /* STATS_H_ */

