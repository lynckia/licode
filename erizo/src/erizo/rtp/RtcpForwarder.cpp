/*
 * RtcpForwarder.cpp
 */

#include "RtcpForwarder.h"
#include <string.h>

namespace erizo{
  DEFINE_LOGGER(RtcpForwarder, "rtp.RtcpForwarder");

  RtcpForwarder::RtcpForwarder (MediaSink* msink, MediaSource* msource, uint32_t maxVideoBw):
    RtcpProcessor(msink, msource, maxVideoBw), defaultVideoBw_(maxVideoBw/2){
    ELOG_DEBUG("Starting RtcpForwarder");
  }

  void RtcpForwarder::addSourceSsrc(uint32_t ssrc){
    boost::mutex::scoped_lock mlock(mapLock_);
    if (rtcpData_.find(ssrc) == rtcpData_.end()){
      this->rtcpData_[ssrc] = boost::shared_ptr<RtcpData>(new RtcpData());
      if (ssrc == this->rtcpSource_->getAudioSourceSSRC()){
        ELOG_DEBUG("It is an audio SSRC %u", ssrc);
        this->rtcpData_[ssrc]->mediaType = AUDIO_TYPE; 
      }else{
        ELOG_DEBUG("It is a video SSRC %u", ssrc);
        this->rtcpData_[ssrc]->mediaType = VIDEO_TYPE;
      }
    }
  }

  void RtcpForwarder::setMaxVideoBW(uint32_t bandwidth){
    this->maxVideoBw_ = bandwidth;
  }

  void RtcpForwarder::setPublisherBW(uint32_t bandwidth){
  }
  
  void RtcpForwarder::analyzeSr(RtcpHeader* chead){
    uint32_t recvSSRC = chead->getSSRC();
    // We try to add it just in case it is not there yet (otherwise its noop)
    this->addSourceSsrc(recvSSRC);

    boost::mutex::scoped_lock mlock(mapLock_);
    boost::shared_ptr<RtcpData> theData = rtcpData_[recvSSRC];
    boost::mutex::scoped_lock lock(theData->dataLock);
    struct timeval now;
    gettimeofday(&now, NULL);
    uint32_t ntp;
    uint64_t theNTP = chead->getNtpTimestamp();
    ntp = (theNTP & (0xFFFFFFFF0000))>>16;
    theData->senderReports.push_back(boost::shared_ptr<SrData>( new SrData(ntp, now)));
    // We only store the last 20 sr
    if (theData->senderReports.size()>20){
      theData->senderReports.pop_front();
    }

  }
  int RtcpForwarder::analyzeFeedback(char *buf, int len) {

    RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(buf);
    if (chead->isFeedback()) {      
      uint32_t sourceSsrc = chead->getSourceSSRC();
      // We try to add it just in case it is not there yet (otherwise its noop)
      this->addSourceSsrc(sourceSsrc);

      boost::mutex::scoped_lock mlock(mapLock_);
      boost::shared_ptr<RtcpData> theData = rtcpData_[sourceSsrc];
      boost::mutex::scoped_lock lock(theData->dataLock);
      struct timeval now;
      gettimeofday(&now, NULL);
      char* movingBuf = buf;
      int rtcpLength = 0;
      int totalLength = 0;
      int partNum = 0;

      do {
        movingBuf+=rtcpLength;
        chead = reinterpret_cast<RtcpHeader*>(movingBuf);
        rtcpLength = (ntohs(chead->length)+1) * 4;
        totalLength += rtcpLength;
        switch(chead->packettype){
          case RTCP_SDES_PT:
            ELOG_DEBUG("SDES");
            break;
          case RTCP_BYE:
            ELOG_DEBUG("BYE");
            break;
          case RTCP_Receiver_PT:
            theData->requestRr = true;
            theData->rrsReceivedInPeriod++;
            if (chead->getSourceSSRC() == rtcpSource_->getVideoSourceSSRC()){
              ELOG_DEBUG("Analyzing Video RR: PacketLost %u, Ratio %u, partNum %d, blocks %d, sourceSSRC %u, ssrc %u", chead->getLostPackets(), chead->getFractionLost(), partNum, chead->getBlockCount(), chead->getSourceSSRC(), chead->getSSRC());
            }else{
              ELOG_DEBUG("Analyzing Audio RR: PacketLost %u, Ratio %u, partNum %d, blocks %d, sourceSSRC %u, ssrc %u", chead->getLostPackets(), chead->getFractionLost(), partNum, chead->getBlockCount(), chead->getSourceSSRC(), chead->getSSRC());
            }
            theData->ratioLost = chead->getFractionLost();  
            theData->totalPacketsLost = chead->getLostPackets();
            theData->highestSeqNumReceived = chead->getHighestSeqnum();
            theData->seqNumCycles = chead->getSeqnumCycles();
            theData->jitter = chead->getJitter();
            theData->lastSr = chead->getLastSr();
            theData->delaySinceLastSr = chead->getDelaySinceLastSr();
            break;
          case RTCP_RTP_Feedback_PT:
            ELOG_DEBUG("RTP FB: Usually NACKs: %u, partNum %d", chead->getBlockCount(), partNum);
            ELOG_DEBUG("NACK PID %u BLP %u", chead->getNackPid(), chead->getNackBlp());
            // We analyze NACK to avoid sending repeated NACKs
            theData->shouldSendNACK = true;
            theData->nackSeqnum = chead->getNackPid();
            theData->nackBlp = chead->getNackBlp();
            break;
          case RTCP_PS_Feedback_PT:
            //            ELOG_DEBUG("RTCP PS FB TYPE: %u", chead->getBlockCount() );
            switch(chead->getBlockCount()){
              case RTCP_PLI_FMT:
                ELOG_DEBUG("PLI Message, partNum %d", partNum);
                // 1: PLI, 4: FIR
                rtcpSource_->sendPLI();

                break;
              case RTCP_SLI_FMT:
                ELOG_DEBUG("SLI Message");
                break;
              case RTCP_FIR_FMT:
                ELOG_DEBUG("FIR Message");
                break;
              case RTCP_AFB:
                {
                  char *uniqueId = (char*)&chead->report.rembPacket.uniqueid;
                  if (!strncmp(uniqueId,"REMB", 4)){
                    uint64_t bitrate = chead->getBrMantis() << chead->getBrExp();
                    ELOG_DEBUG("Received REMB %lu", bitrate);
                    if (bitrate < maxVideoBw_){
                      theData->reportedBandwidth = bitrate;
                      theData->shouldSendREMB = true;
                    }else{
                      theData->reportedBandwidth = maxVideoBw_;
                    }
                    chead->setREMBBitRate(theData->reportedBandwidth);
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

        partNum++;
      } while (totalLength < len);
      return len;
    }
    return 0;
  }


  void RtcpForwarder::checkRtcpFb(){
    /*
    boost::mutex::scoped_lock mlock(mapLock_);
    std::map<uint32_t, boost::shared_ptr<RtcpData>>::iterator it;
    for (it = rtcpData_.begin(); it != rtcpData_.end(); it++){
      boost::shared_ptr<RtcpData> rtcpData = it->second;
      boost::mutex::scoped_lock lock(rtcpData->dataLock);
      uint32_t sourceSsrc = it->first;      
      uint32_t sinkSsrc;
      struct timeval now;
      gettimeofday(&now, NULL);

      if((rtcpData->requestRr || rtcpData->shouldSendREMB)){  // Generate Full RTCP Packet
        rtcpData->requestRr = false;
        RtcpHeader rtcpHead;
        rtcpHead.setPacketType(RTCP_Receiver_PT);
        if (sourceSsrc == rtcpSource_->getAudioSourceSSRC()){
          sinkSsrc = rtcpSink_->getAudioSinkSSRC();
          ELOG_DEBUG("Sending Audio RR: SOurcessrc %u sinkSsrc %u PacketLost %u, Ratio %u, DLSR %u, lastSR %u, rrsSinceLast %u highestSeqNum %u jitter %u", sourceSsrc, rtcpSink_->getAudioSinkSSRC(), rtcpData->totalPacketsLost, rtcpData->ratioLost, rtcpData->delaySinceLastSr, rtcpData->lastSr, rtcpData->rrsReceivedInPeriod, rtcpData->highestSeqNumReceived, rtcpData->jitter);
          rtcpHead.setSSRC(rtcpSink_->getAudioSinkSSRC());
          rtcpHead.setSourceSSRC(rtcpSource_->getAudioSourceSSRC());
        }else{
          sinkSsrc = rtcpSink_->getVideoSinkSSRC();
          ELOG_DEBUG("Sending Video RR: SOurcessrc %u sinkSsrc %u PacketLost %u, Ratio %u, DLSR %u, lastSR %u, rrsSinceLast %u highestSeqNum %u jitter %u", sourceSsrc, rtcpSink_->getVideoSinkSSRC(), rtcpData->totalPacketsLost, rtcpData->ratioLost, rtcpData->delaySinceLastSr, rtcpData->lastSr, rtcpData->rrsReceivedInPeriod, rtcpData->highestSeqNumReceived, rtcpData->jitter);
          rtcpHead.setSSRC(rtcpSink_->getVideoSinkSSRC());
          rtcpHead.setSourceSSRC(rtcpSource_->getVideoSourceSSRC());
        }

        rtcpHead.setHighestSeqnum(rtcpData->highestSeqNumReceived);      
        rtcpHead.setSeqnumCycles(rtcpData->seqNumCycles);
        rtcpHead.setLostPackets(rtcpData->totalPacketsLost);
        rtcpHead.setJitter(rtcpData->jitter);
        rtcpHead.setLastSr(rtcpData->lastSr);
        rtcpHead.setDelaySinceLastSr(rtcpData->delaySinceLastSr);
        rtcpHead.setLength(7);
        rtcpHead.setBlockCount(1);
        int length = (rtcpHead.getLength()+1)*4;
        memcpy(packet_, (uint8_t*)&rtcpHead, length);
        if(rtcpData->shouldSendNACK){
          ELOG_DEBUG("SEND NACK, SENDING with Seqno: %u", rtcpData->nackSeqnum);
          int theLen = this->addNACK((char*)packet_, length, rtcpData->nackSeqnum, rtcpData->nackBlp, sourceSsrc, sinkSsrc);
          rtcpData->shouldSendNACK = false;
          length = theLen;
        }
        if  (sourceSsrc == rtcpSource_->getVideoSourceSSRC()){
          rtcpSink_->deliverVideoData((char*)packet_, length);
        }else{
          rtcpSink_->deliverAudioData((char*)packet_, length);
        }
        rtcpData->lastRrSent = now;
        if (rtcpData->shouldSendPli){
          rtcpSource_->sendPLI();
          rtcpData->shouldSendPli = false;
        }
      }
    }
    */
  }

  int RtcpForwarder::addREMB(char* buf, int len, uint32_t bitrate){
    buf+=len;
    RtcpHeader theREMB;
    theREMB.setPacketType(RTCP_PS_Feedback_PT);
    theREMB.setBlockCount(RTCP_AFB);
    memcpy(&theREMB.report.rembPacket.uniqueid, "REMB", 4);

    theREMB.setSSRC(rtcpSink_->getVideoSinkSSRC());
    theREMB.setSourceSSRC(rtcpSource_->getVideoSourceSSRC());
    theREMB.setLength(5);
    theREMB.setREMBBitRate(bitrate);
    theREMB.setREMBNumSSRC(1);
    theREMB.setREMBFeedSSRC(rtcpSource_->getVideoSourceSSRC());
    int rembLength = (theREMB.getLength()+1)*4;

    memcpy(buf, (uint8_t*)&theREMB, rembLength);
    return (len+rembLength); 
  }

  int RtcpForwarder::addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc, uint32_t sinkSsrc){
    buf+=len;
    ELOG_DEBUG("Setting PID %u, BLP %u", seqNum , blp);
    RtcpHeader theNACK;
    theNACK.setPacketType(RTCP_RTP_Feedback_PT);
    theNACK.setBlockCount(1);
    theNACK.setNackPid(seqNum);
    theNACK.setNackBlp(blp);
    theNACK.setSSRC(sinkSsrc);    
    theNACK.setSourceSSRC(sourceSsrc);
    theNACK.setLength(3);
    int nackLength = (theNACK.getLength()+1)*4;

    memcpy(buf, (uint8_t*)&theNACK, nackLength);
    return (len+nackLength); 
  }


} /* namespace erizo */
