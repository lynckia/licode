/*
 * Stats.cpp
 *
 */

#include <sstream>

#include "Stats.h"
#include "WebRtcConnection.h"

namespace erizo {

  DEFINE_LOGGER(Stats, "Stats");
  
  Stats::Stats(){ 
    ELOG_DEBUG("Constructor Stats");
    theListener_ = NULL;
  }
  Stats::~Stats(){
    ELOG_DEBUG("Destructor Stats");
  }

  void Stats::processRtcpPacket(char* buf, int length) {
    boost::recursive_mutex::scoped_lock lock(mapMutex_);
    char* movingBuf = buf;
    int rtcpLength = 0;
    int totalLength = 0;
    
    do{
      movingBuf+=rtcpLength;
      RtcpHeader *chead= reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength= (ntohs(chead->length)+1)*4;      
      totalLength+= rtcpLength;
      this->processRtcpPacket(chead);
    } while(totalLength<length);
    sendStats();
  }
  
  void Stats::processRtcpPacket(RtcpHeader* chead) {    
    unsigned int ssrc = chead->getSSRC();

    ELOG_DEBUG("RTCP SubPacket: PT %d, SSRC %u,  block count %d ",chead->packettype,chead->getSSRC(), chead->getBlockCount()); 
    switch(chead->packettype){
      case RTCP_SDES_PT:
        ELOG_DEBUG("SDES");
        break;
      case RTCP_Receiver_PT:
        setFractionLost (chead->getFractionLost(), ssrc);
        setPacketsLost (chead->getLostPackets(), ssrc);
        setJitter (chead->getJitter(), ssrc);
        setSourceSSRC(chead->getSourceSSRC(), ssrc);
        break;
      case RTCP_Sender_PT:
        setRtcpPacketSent(chead->getPacketsSent(), ssrc);
        setRtcpBytesSent(chead->getOctetsSent(), ssrc);
        break;
      case RTCP_RTP_Feedback_PT:
        ELOG_DEBUG("RTP FB: Usually NACKs: %u", chead->getBlockCount());
        ELOG_DEBUG("PID %u BLP %u", chead->getNackPid(), chead->getNackBlp());
        accountNACKMessage(ssrc);
        break;
      case RTCP_PS_Feedback_PT:
        ELOG_DEBUG("RTCP PS FB TYPE: %u", chead->getBlockCount() );
        switch(chead->getBlockCount()){
          case RTCP_PLI_FMT:
            ELOG_DEBUG("PLI Message");
            accountPLIMessage(ssrc);
            break;
          case RTCP_SLI_FMT:
            ELOG_DEBUG("SLI Message");
            accountSLIMessage(ssrc);
            break;
          case RTCP_FIR_FMT:
            ELOG_DEBUG("FIR Message");
            accountFIRMessage(ssrc);
            break;
          case RTCP_AFB:
            {
              char *uniqueId = (char*)&chead->report.rembPacket.uniqueid;
              if (!strncmp(uniqueId,"REMB", 4)){
                uint64_t bitrate = chead->getBrMantis() << chead->getBrExp();
                ELOG_DEBUG("REMB Packet numSSRC %u mantissa %u exp %u, tot %lu bps", chead->getREMBNumSSRC(), chead->getBrMantis(), chead->getBrExp(), bitrate);
                setBandwidth(bitrate, ssrc);
              }
              else{
                ELOG_DEBUG("Unsupported AFB Packet not REMB")
              }
              break;
            }
          default:
            ELOG_WARN("Unsupported RTCP_PS FB TYPE %u",chead->getBlockCount());
            break;
        }
        break;
      default:
        ELOG_DEBUG("Unknown RTCP Packet, %d", chead->packettype);
        break;
    }
  }
 
  std::string Stats::getStats() {
    boost::recursive_mutex::scoped_lock lock(mapMutex_);
    std::ostringstream theString;
    theString << "[";
    for (fullStatsMap_t::iterator itssrc=statsPacket_.begin(); itssrc!=statsPacket_.end();){
      unsigned long int currentSSRC = itssrc->first;
      theString << "{\"ssrc\":\"" << currentSSRC << "\",\n";
      if (currentSSRC == videoSSRC_){
        theString << "\"type\":\"" << "video\",\n";
      }else if (currentSSRC == audioSSRC_){
        theString << "\"type\":\"" << "audio\",\n";
      }
      for (singleSSRCstatsMap_t::iterator it=statsPacket_[currentSSRC].begin(); it!=statsPacket_[currentSSRC].end();){
        theString << "\"" << it->first << "\":\"" << it->second << "\"";
        if (++it != statsPacket_[currentSSRC].end()){
          theString << ",\n";
        }          
      }
      theString << "}";
      if (++itssrc != statsPacket_.end()){
        theString << ",";
      }
    }
    theString << "]";
    return theString.str(); 
  }
  
  void Stats::sendStats() {
    if(theListener_!=NULL)
      theListener_->notifyStats(this->getStats());
  }
}

