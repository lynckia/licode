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
    
    Stats();
    virtual ~Stats();

    void processRtcpPacket(char* buf, int length);
    std::string getStats();
    // The video and audio SSRCs of the Client
    void setVideoSourceSSRC(unsigned int ssrc){
      videoSSRC_ = ssrc;
    };
    void setAudioSourceSSRC(unsigned int ssrc){
      audioSSRC_ = ssrc;
    };
    inline void setStatsListener(WebRtcConnectionStatsListener* listener){
      this->theListener_ = listener;
    }
    
    private:
    typedef std::map<std::string, unsigned long int> singleSSRCstatsMap_t;
    typedef std::map <unsigned long int, singleSSRCstatsMap_t> fullStatsMap_t;
    fullStatsMap_t theStats_;
    static const int SLEEP_INTERVAL_ = 100000;
    boost::recursive_mutex mapMutex_;
    WebRtcConnectionStatsListener* theListener_;
    unsigned int videoSSRC_, audioSSRC_;

    void processRtcpPacket(RtcpHeader* chead);


    int getPacketsLost(unsigned int ssrc){
      return static_cast<int>(theStats_[ssrc]["packetsLost"]);
    };
    void setPacketsLost(int packets, unsigned int ssrc){
      theStats_[ssrc]["packetsLost"] = static_cast<unsigned int>(packets);
    };

    unsigned int getFractionLost(unsigned int ssrc){
      return theStats_[ssrc]["fractionLost"];
    };
    void setFractionLost(unsigned int fragment, unsigned int SSRC){
      theStats_[SSRC]["fractionLost"] = fragment;
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
    unsigned int getSourceSSRC (unsigned int sourceSSRC, unsigned int ssrc){
      return theStats_[ssrc]["sourceSsrc"];
    };
    void setSourceSSRC (unsigned int sourceSSRC, unsigned int ssrc){
      theStats_[ssrc]["sourceSsrc"] = sourceSSRC;
    };

    void sendStats();
  };

}

#endif /* STATS_H_ */

