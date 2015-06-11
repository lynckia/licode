/*
 * RtcpProcessor.cpp
 */

#include "RtcpProcessor.h"

namespace erizo{
  DEFINE_LOGGER(RtcpProcessor, "rtp.RtcpProcessor");

  void RtcpData::reset(uint32_t bandwidth){
    ratioLost = 0;
    requestRr = false;
    shouldReset = false;
    jitter = 0;
    rrsReceivedInPeriod = 0;
    if (reportedBandwidth < bandwidth){
      reportedBandwidth = reportedBandwidth*2 < bandwidth?reportedBandwidth*2:bandwidth;
    }
    lastDelay = lastDelay*0.6;
  }

  RtcpProcessor::RtcpProcessor (MediaSink* msink, MediaSource* msource, int defaultBw):
    rtcpSink_(msink), rtcpSource_(msource), defaultBw_(defaultBw){
    ELOG_DEBUG("Starting RtcpProcessor");
  }

  void RtcpProcessor::addSourceSsrc(uint32_t ssrc){
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

  void RtcpProcessor::setVideoBW(uint32_t bandwidth){
    ELOG_DEBUG("Setting Video BW %u", bandwidth);
    this->defaultBw_ = bandwidth;
  }

  void RtcpProcessor::analyzeSr(RtcpHeader* chead){
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
  void RtcpProcessor::analyzeFeedback(char *buf, int len) {

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
      uint32_t calculatedlsr, delay, calculateLastSr;

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
            theData->rrsReceivedInPeriod++;
            if (chead->getSourceSSRC() == rtcpSource_->getVideoSourceSSRC()){
              ELOG_DEBUG("Analyzing Video RR: PacketLost %u, Ratio %u, partNum %d, blocks %d, sourceSSRC %u", chead->getLostPackets(), chead->getFractionLost(), partNum, chead->getBlockCount(), chead->getSourceSSRC());
            }else{
              ELOG_DEBUG("Analyzing Audio RR: PacketLost %u, Ratio %u, partNum %d, blocks %d, sourceSSRC %u", chead->getLostPackets(), chead->getFractionLost(), partNum, chead->getBlockCount(), chead->getSourceSSRC());
            }
            theData->ratioLost = theData->ratioLost > chead->getFractionLost()? theData->ratioLost: chead->getFractionLost();  
            theData->totalPacketsLost = theData->totalPacketsLost > chead->getLostPackets()? theData->totalPacketsLost : chead->getLostPackets();
            theData->highestSeqNumReceived = theData->highestSeqNumReceived > chead->getHighestSeqnum()? theData->highestSeqNumReceived : chead->getHighestSeqnum();
            theData->jitter = theData->jitter > chead->getJitter()? theData->jitter: chead->getJitter();
            calculateLastSr = chead->getLastSr();
            calculatedlsr = (chead->getDelaySinceLastSr()*1000)/65536;
            for (std::list<boost::shared_ptr<SrData>>::iterator it=theData->senderReports.begin(); it != theData->senderReports.end(); ++it){
              if ((*it)->srNtp == calculateLastSr){  
                uint64_t nowms = (now.tv_sec * 1000) + (now.tv_usec / 1000);
                uint64_t sentts = ((*it)->timestamp.tv_sec * 1000) + ((*it)->timestamp.tv_usec / 1000);
                delay = nowms - sentts - calculatedlsr;

              }
            }

            if (theData->lastSr==0||theData->lastDelay < delay){
              ELOG_DEBUG("Recording DLSR %u, lastSR %u last delay %u, calculated delay %u for SSRC %u", chead->getDelaySinceLastSr(), chead->getLastSr(), theData->lastDelay, delay, sourceSsrc);
              theData->lastSr = chead->getLastSr();
              theData->delaySinceLastSr = chead->getDelaySinceLastSr();
              theData->lastSrUpdated = now;
              theData->lastDelay = delay;
            }else{
              //              ELOG_DEBUG("Not recording delay %u, lastDelay %u", delay, theData->lastDelay);
            }
            break;
          case RTCP_RTP_Feedback_PT:
            ELOG_DEBUG("RTP FB: Usually NACKs: %u, partNum %d", chead->getBlockCount(), partNum);
            ELOG_DEBUG("PID %u BLP %u", chead->getNackPid(), chead->getNackBlp());
            theData->shouldSendNACK = true;
            theData->nackSeqnum = chead->getNackPid();
            theData->nackBlp = chead->getNackBlp();
            theData->requestRr = true;
            break;
          case RTCP_PS_Feedback_PT:
            //            ELOG_DEBUG("RTCP PS FB TYPE: %u", chead->getBlockCount() );
            switch(chead->getBlockCount()){
              case RTCP_PLI_FMT:
                ELOG_DEBUG("PLI Message, partNum %d", partNum);
                // 1: PLI, 4: FIR
                theData->shouldSendPli = true;
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
                    if ((bitrate<theData->reportedBandwidth) || theData->reportedBandwidth==0){
                      ELOG_DEBUG("Should send Packet REMB, before BR %lu, will send with Br %lu", theData->reportedBandwidth, bitrate);
                      theData->reportedBandwidth = bitrate;  
                      theData->shouldSendREMB = true;
                    }
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
    }
  }


  void RtcpProcessor::checkRtcpFb(){
    boost::mutex::scoped_lock mlock(mapLock_);
    std::map<uint32_t, boost::shared_ptr<RtcpData>>::iterator it;
    for (it = rtcpData_.begin(); it != rtcpData_.end(); it++){
      boost::shared_ptr<RtcpData> rtcpData = it->second;
      boost::mutex::scoped_lock lock(rtcpData->dataLock);
      uint32_t sourceSsrc = it->first;      
      uint32_t sinkSsrc;
      struct timeval now;
      gettimeofday(&now, NULL);

      unsigned int dt = (now.tv_sec - rtcpData->lastRrSent.tv_sec) * 1000 + (now.tv_usec - rtcpData->lastRrSent.tv_usec) / 1000;
      unsigned int edlsr = (now.tv_sec - rtcpData->lastSrUpdated.tv_sec) * 1000 + (now.tv_usec - rtcpData->lastSrUpdated.tv_usec) / 1000;
      unsigned int dtScheduled = (now.tv_sec - rtcpData->lastRrWasScheduled.tv_sec) * 1000 + (now.tv_usec - rtcpData->lastRrWasScheduled.tv_usec) / 1000;

      if((rtcpData->requestRr||dt >= rtcpData->nextPacketInMs ||rtcpData->shouldSendREMB) && rtcpData->lastSr > 0){  // Generate Full RTCP Packet
        rtcpData->requestRr = false;
        RtcpHeader rtcpHead;
        rtcpHead.setPacketType(RTCP_Receiver_PT);
        if (sourceSsrc == rtcpSource_->getAudioSourceSSRC()){
          sinkSsrc = rtcpSink_->getAudioSinkSSRC();
          ELOG_DEBUG("Sending Audio RR: PacketLost %u, Ratio %u, DLSR %u, lastSR %u, DLSR Adjusted %u dt %u, rrsSinceLast %u highestSeqNum %u", rtcpData->totalPacketsLost, rtcpData->ratioLost, rtcpData->delaySinceLastSr, rtcpData->lastSr, (rtcpData->delaySinceLastSr+edlsr), dt, rtcpData->rrsReceivedInPeriod, rtcpData->highestSeqNumReceived);
          rtcpHead.setSSRC(rtcpSink_->getAudioSinkSSRC());
          rtcpHead.setSourceSSRC(rtcpSource_->getAudioSourceSSRC());
        }else{
          sinkSsrc = rtcpSink_->getVideoSinkSSRC();
          ELOG_DEBUG("Sending Video RR: SOurcessrc %u sinkSsrc %u PacketLost %u, Ratio %u, DLSR %u, lastSR %u, DLSR Adjusted %u dt %u, rrsSinceLast %u highestSeqNum %u jitter %u", sourceSsrc, rtcpSink_->getVideoSinkSSRC(), rtcpData->totalPacketsLost, rtcpData->ratioLost, rtcpData->delaySinceLastSr, rtcpData->lastSr, (rtcpData->delaySinceLastSr+edlsr), dt, rtcpData->rrsReceivedInPeriod, rtcpData->highestSeqNumReceived, rtcpData->jitter);
          rtcpHead.setSSRC(rtcpSink_->getVideoSinkSSRC());
          rtcpHead.setSourceSSRC(rtcpSource_->getVideoSourceSSRC());
        }
        rtcpHead.setFractionLost(rtcpData->ratioLost);
        rtcpHead.setHighestSeqnum(rtcpData->highestSeqNumReceived);      
        rtcpHead.setLostPackets(rtcpData->totalPacketsLost);
        rtcpHead.setJitter(rtcpData->jitter);
        rtcpHead.setLastSr(rtcpData->lastSr);
        rtcpHead.setDelaySinceLastSr(rtcpData->delaySinceLastSr + ((edlsr*65536)/1000));
        rtcpHead.setLength(7);
        rtcpHead.setBlockCount(1);
        int length = (rtcpHead.getLength()+1)*4;
        memcpy(packet_, (uint8_t*)&rtcpHead, length);
        if(rtcpData->shouldSendNACK){
          ELOG_DEBUG("SEND NACK, SENDING with Seqno: %u", rtcpData->nackSeqnum);
          int theLen = this->addNACK((char*)packet_, length, rtcpData->nackSeqnum, rtcpData->nackBlp, sourceSsrc, sinkSsrc);
          rtcpData->shouldSendNACK = false;
          rtcpData->nackSeqnum = 0;
          rtcpData->nackBlp = 0;
          length = theLen;
        }
        if (rtcpData->mediaType == VIDEO_TYPE){
          unsigned int sincelastREMB = (now.tv_sec - rtcpData->lastREMBSent.tv_sec) * 1000 + (now.tv_usec - rtcpData->lastREMBSent.tv_usec) / 1000;
          if (sincelastREMB > REMB_TIMEOUT){
            rtcpData->shouldSendREMB = true;
          }

          if(rtcpData->shouldSendREMB ){
            ELOG_DEBUG("Sending Remb, since last %u ms, sending with BW: %lu", sincelastREMB, rtcpData->reportedBandwidth);
            int theLen = this->addREMB((char*)packet_, length, rtcpData->reportedBandwidth);
            rtcpData->shouldSendREMB = false;
            rtcpData->lastREMBSent = now;
            length = theLen;
          }
        }
        if  (sourceSsrc == rtcpSource_->getVideoSourceSSRC()){
          rtcpSink_->deliverVideoData((char*)packet_, length);
        }else{
          rtcpSink_->deliverAudioData((char*)packet_, length);
        }
        rtcpData->lastRrSent = now;
        if (dtScheduled>rtcpData->nextPacketInMs) // Every scheduled packet we reset
          rtcpData->shouldReset = true;
          rtcpData->lastRrWasScheduled = now;
        // schedule next packet
        float random = (rand()%100+50)/100.0;
        if ( rtcpData->mediaType == AUDIO_TYPE){
          rtcpData->nextPacketInMs = RR_AUDIO_PERIOD*random;
          ELOG_DEBUG("Scheduled next Audio RR in %u ms", rtcpData->nextPacketInMs);
        } else {
          rtcpData->nextPacketInMs = (RR_VIDEO_BASE)*random;
          ELOG_DEBUG("Scheduled next Video RR in %u ms", rtcpData->nextPacketInMs);
        }

        if (rtcpData->shouldReset){
          rtcpData->reset(this->defaultBw_);
        }
      }
      if (rtcpData->shouldSendPli){
        rtcpSource_->sendPLI();
        rtcpData->shouldSendPli = false;
      }
    }
  }

  int RtcpProcessor::addREMB(char* buf, int len, uint32_t bitrate){
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

  int RtcpProcessor::addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc, uint32_t sinkSsrc){
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
