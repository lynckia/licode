/*
 * Stats.h
 */

#ifndef STATS_H_
#define STATS_H_

#include <string>
#include <map>

namespace erizo{

  class Stats{
   
    public:
      Stats(unsigned long int ssrc=0):SSRC_(ssrc){
        theStats_["packetsLost"]=0;
        theStats_["fragmentLost"]=0;
        theStats_["rtcpPacketCount"]=0;
        theStats_["rtcpBytesCount"]=0;
      };
      unsigned int getPacketsLost(){
        return theStats_["packetsLost"];
      };
      void setPacketsLost(unsigned int packets){
        theStats_["packetsLost"] = packets;
      };
      
      unsigned int getFragmentLost(){
        return theStats_["packetsLost"];
      };
      void setFragmentLost(unsigned int fragment){
        theStats_["fragmentLost"] = fragment;
      };

      unsigned int getRtcpPacketCount(){
        return theStats_["rtcpPacketCount"];
      };
      void setRtcpPacketCount(unsigned int count){
        theStats_["rtcpPacketCount"] = count;
      };

      unsigned int getRtcpBytesCount(){
        return theStats_["rtcpBytesCount"];
      };
      void setRtcpBytesCount(unsigned int count){
        theStats_["rtcpBytesCount"] = count;
      };
    
      std::string getString();

    private:
      std::map <std::string, unsigned int> theStats_;
      unsigned long int SSRC_;
  };

}

#endif /* STATS_H_ */

