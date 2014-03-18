/*
 * Stats.cpp
 *
 */

#include <sstream>

#include "Stats.h"
#include "WebRtcConnection.h"

namespace erizo {

  DEFINE_LOGGER(Stats, "Stats");

  Stats::~Stats(){
    if (runningStats_){
      runningStats_ = false;
      statsThread_.join();
      ELOG_DEBUG("Stopped periodic stats report");
    }
  }
  
  void Stats::processRtcpStats(RtcpHeader* chead) {    
    boost::mutex::scoped_lock lock(mapMutex_);
    if (chead->packettype == RTCP_Receiver_PT){
      addFragmentLost (chead->getFractionLost());
      setPacketsLost (chead->getLostPackets());
      setJitter (chead->getJitter());
    }else if (chead->packettype == RTCP_Sender_PT){
      setRtcpPacketSent(chead->getPacketsSent());
      setRtcpBytesSent(chead->getOctetsSent());
    }
  }
  
  std::string Stats::getStats() {    
    boost::mutex::scoped_lock lock(mapMutex_);
    std::ostringstream theString;
    theString << "{";
    for (std::map<std::string, unsigned int>::iterator it=theStats_.begin(); it!=theStats_.end(); ++it){
      theString << "\"" << it->first << "\":\"" << it->second << "\"";
      if (++it != theStats_.end()){
        theString << ",\n";
      }
      --it;
    }
    theString << "\n}";

    std::map<std::string, unsigned int>::iterator search = theStats_.find("fragmentLost");
    if (search != theStats_.end()) {
      search->second = 0;
    }
    return theString.str(); 
  }

  void Stats::setPeriodicStats(int intervalMillis, WebRtcConnectionStatsListener* listener) {
    theListener_ = listener;
    interval_ = intervalMillis*1000;
    runningStats_ = true;
    ELOG_DEBUG("Starting periodic stats report with interval %d", intervalMillis);
    statsThread_ = boost::thread(&Stats::sendStats, this);
  }

  void Stats::sendStats(){
    while(runningStats_){
      theListener_->notifyStats(this->getStats());
      usleep(interval_);
    }
  }
}

