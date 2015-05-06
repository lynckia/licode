/*
 * RtcpProcessor.cpp
 */

#include "RtcpProcessor.h"

namespace erizo{
  DEFINE_LOGGER(RtcpProcessor, "RtcpProcessor");

  RtcpProcessor::RtcpProcessor (MediaSink* msink, MediaSource* msource, int defaultBw):
    rtcpSink_(msink), rtcpSource_(msource), defaultBw_(defaultBw){
    ELOG_DEBUG("Starting RtcpProcessor");
  }

  void RtcpProcessor::addSourceSsrc(uint32_t ssrc){
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
    this->defaultBw_ = bandwidth;
  }

  void RtcpProcessor::analyzeSr(RtcpHeader* chead){
    uint32_t recvSSRC = chead->getSSRC();
    if (rtcpData_.find(recvSSRC) == rtcpData_.end()){
      ELOG_DEBUG("SR RtcpData structure not yet created, creating, SSRC %u", recvSSRC);
      this->addSourceSsrc(recvSSRC);
    }

    boost::shared_ptr<RtcpData> theData = rtcpData_[recvSSRC];
    boost::mutex::scoped_lock lock(theData->dataLock);
    struct timeval now;
    gettimeofday(&now, NULL);
    uint32_t ntp;
    uint64_t theNTP = chead->getNtpTimestamp();
    ntp = (theNTP & (0xFFFFFFFF0000))>>16;
    theData->senderReports.push_back(boost::shared_ptr<SrData>( new SrData(ntp, now)));
    // We only store the last 40 sr
    if (theData->senderReports.size()>40){
      theData->senderReports.pop_front();
    }

  }
  void RtcpProcessor::analyzeFeedback(char *buf, int len) {

    RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(buf);
    if (chead->isFeedback()) {      
      uint32_t sourceSsrc = chead->getSourceSSRC();
      if (rtcpData_.find(sourceSsrc) == rtcpData_.end()){
        ELOG_DEBUG("RR RtcpData structure not yet created, creating %u", sourceSsrc);
        this->addSourceSsrc(sourceSsrc);
      }
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
        theData->lastUpdated = now;
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
              //              ELOG_DEBUG("Analyzing Video RR: PacketLost %u, Ratio %u, partNum %d, blocks %d, sourceSSRC %u", chead->getLostPackets(), chead->getFractionLost(), partNum, chead->getBlockCount(), chead->getSourceSSRC());
            }else{
              //              ELOG_DEBUG("Analyzing Audio RR: PacketLost %u, Ratio %u, partNum %d, blocks %d, sourceSSRC %u", chead->getLostPackets(), chead->getFractionLost(), partNum, chead->getBlockCount(), chead->getSourceSSRC());
            }
            theData->ratioLost = theData->ratioLost > chead->getFractionLost()? theData->ratioLost: chead->getFractionLost();  
            theData->totalPacketsLost = theData->totalPacketsLost > chead->getLostPackets()? theData->totalPacketsLost : chead->getLostPackets();
            theData->highestSeqNumReceived = theData->highestSeqNumReceived < chead->getHighestSeqnum()? theData->highestSeqNumReceived : chead->getHighestSeqnum();
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
            //            ELOG_DEBUG("RTP FB: Usually NACKs: %u, partNum %d", chead->getBlockCount(), partNum);
            //            ELOG_DEBUG("PID %u BLP %u", chead->getNackPid(), chead->getNackBlp());
            theData->shouldSendNACK = true;
            theData->nackSeqnum = chead->getNackPid();
            theData->nackBlp = chead->getNackBlp();
            theData->requestRr = true;
            // NACK packet!
            /*
               int len = chead->getLength() - 2;
               for (int k = 0; k < len; ++k) {
               uint16_t seqNum = ntohs(*((uint16_t*)(movingBuf + 12 + k * 4)));
               addNackPacket(seqNum, pData);
               uint16_t blp = ntohs(*((uint16_t*)(movingBuf + 14 + k * 4)));
               ELOG_DEBUG("FeedbackPT: NACK %x - and: %x", seqNum, blp);
               uint16_t bitmask = 1;
               for (int i = 0; i < 16; ++i) {
               if ((blp & bitmask) != 0) {
               boost::mutex::scoped_lock lock(rtpPacketLock_);
               int cur = seqNum + i;
               bool resent = false;
               if (pMediaSink != NULL && rtpPacketMemory_.size() > 0) {
               int diff = rtpPacketMemory_.at(0).seqNumber - cur;
               if (diff > 0x7fff) {
               diff -= 0x10000;
               } else if (diff < 0) {
               diff += 0x10000;
               }
               if (0 <= diff && diff < rtpPacketMemory_.size()) {
               if (rtpPacketMemory_[diff].isSet() && rtpPacketMemory_[diff].seqNumber == (uint16_t)cur) {
               resent = true;
               pMediaSink->deliverVideoData(rtpPacketMemory_[diff].buf, rtpPacketMemory_[diff].len);
               }
               }
               ELOG_DEBUG("resent (%i) packet %x - diff: %i", resent, cur, diff);
               }

               if (!resent) {
               addNackPacket(seqNum + i, pData);
               }
               }
               bitmask *= 2;
               }
               }
               */
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
                    if (theData->reportedBandwidth > bitrate || theData->reportedBandwidth==0){
                   //   ELOG_DEBUG("Should send Packet REMB, before BR %lu, will send with Br %lu", theData->reportedBandwidth, bitrate);
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
    std::map<uint32_t, boost::shared_ptr<RtcpData>>::iterator it;
    for (it = rtcpData_.begin(); it != rtcpData_.end(); it++){
      boost::shared_ptr<RtcpData> rtcpData = it->second;
      boost::mutex::scoped_lock lock(rtcpData->dataLock);
      uint32_t sourceSsrc = it->first;
      struct timeval now;
      gettimeofday(&now, NULL);

      unsigned int dt = (now.tv_sec - rtcpData->lastRrSent.tv_sec) * 1000 + (now.tv_usec - rtcpData->lastRrSent.tv_usec) / 1000;
      unsigned int edlsr = (now.tv_sec - rtcpData->lastSrUpdated.tv_sec) * 1000 + (now.tv_usec - rtcpData->lastSrUpdated.tv_usec) / 1000;

      if((rtcpData->requestRr||dt >= RTCP_PERIOD||rtcpData->shouldSendREMB) && rtcpData->lastSr > 0){  // Generate Full RTCP Packet
        bool shouldReset = false;
        uint8_t packet[128]; // 128 is the max packet length
        rtcpData->requestRr = false;
        RtcpHeader rtcpHead;
        rtcpHead.setPacketType(RTCP_Receiver_PT);
        if (sourceSsrc == rtcpSource_->getAudioSourceSSRC()){
          //          ELOG_DEBUG("Sending Audio RR: PacketLost %u, Ratio %u, DLSR %u, lastSR %u, DLSR Adjusted %u dt %u, rrsSinceLast %u", rtcpData->totalPacketsLost, rtcpData->ratioLost, rtcpData->delaySinceLastSr, rtcpData->lastSr, (rtcpData->delaySinceLastSr+edlsr), dt, rtcpData->rrsReceivedInPeriod);
          rtcpHead.setSSRC(rtcpSink_->getAudioSinkSSRC());
          rtcpHead.setSourceSSRC(rtcpSource_->getAudioSourceSSRC());
        }else{
          //          ELOG_DEBUG("Sending Video RR: SOurcessrc %u sinkSsrc %u PacketLost %u, Ratio %u, DLSR %u, lastSR %u, DLSR Adjusted %u dt %u, rrsSinceLast %u", sourceSsrc, wrtcConn_->getVideoSinkSSRC(), rtcpData->totalPacketsLost, rtcpData->ratioLost, rtcpData->delaySinceLastSr, rtcpData->lastSr, (rtcpData->delaySinceLastSr+edlsr), dt, rtcpData->rrsReceivedInPeriod);
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
        memcpy(packet, (uint8_t*)&rtcpHead, length);
        if(rtcpData->shouldSendNACK){
          //          ELOG_DEBUG("SEND NACK, SENDING with Seqno: %u", rtcpData->nackSeqnum);
          int theLen = this->addNACK((char*)packet, length, rtcpData->nackSeqnum, rtcpData->nackBlp, sourceSsrc);
          rtcpData->shouldSendNACK = false;
          rtcpData->nackSeqnum = 0;
          rtcpData->nackBlp = 0;
          length = theLen;
        }
        unsigned int sincelastREMB = (now.tv_sec - rtcpData->lastREMBSent.tv_sec) * 1000 + (now.tv_usec - rtcpData->lastREMBSent.tv_usec) / 1000;
        if((rtcpData->mediaType == VIDEO_TYPE) && (rtcpData->shouldSendREMB || sincelastREMB > REMB_TIMEOUT)){
          if (sincelastREMB > REMB_TIMEOUT){
            ELOG_DEBUG("Too much time since last REMB, generating one with default BW");
            rtcpData->reportedBandwidth = defaultBw_*1000;
          }
          ELOG_DEBUG("SEND REMB, SINCE LAST %u ms, SENDING with BW: %lu", sincelastREMB, rtcpData->reportedBandwidth);
          int theLen = this->addREMB((char*)packet, length, rtcpData->reportedBandwidth);
          rtcpData->shouldSendREMB = false;
          rtcpData->lastREMBSent = now;
          length = theLen;
        }
        if (dt>=RTCP_PERIOD)
          shouldReset = true;

        // wrtcConn_->queueData(0, (char*)packet, length, videoTransport_, OTHER_PACKET);
        if  (sourceSsrc == rtcpSource_->getVideoSourceSSRC()){
          rtcpSink_->deliverVideoData((char*)packet, length);
        }else{
          rtcpSink_->deliverAudioData((char*)packet, length);
        }
        rtcpData->lastRrSent = now;
        if (shouldReset){
          rtcpData->reset();
        }
      }
      if (rtcpData->shouldSendPli){
        unsigned int sincelast = (now.tv_sec - rtcpData->lastPliSent.tv_sec) * 1000 + (now.tv_usec - rtcpData->lastPliSent.tv_usec) / 1000;
        if (sincelast >= PLI_THRESHOLD){
          ELOG_DEBUG("SENDING PLI");
          rtcpSource_->sendPLI();
          rtcpData->lastPliSent = now;
          rtcpData->shouldSendPli = false;
        }

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

  int RtcpProcessor::addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc){
    buf+=len;
    RtcpHeader theNACK;
    theNACK.setPacketType(RTCP_RTP_Feedback_PT);
    theNACK.setBlockCount(1);
    theNACK.setNackPid(seqNum);
    theNACK.setNackBlp(blp);

    uint32_t ssrc = (sourceSsrc==rtcpSource_->getAudioSourceSSRC())?rtcpSink_->getAudioSinkSSRC():rtcpSink_->getVideoSinkSSRC();
    theNACK.setSSRC(ssrc);    
    theNACK.setSourceSSRC(sourceSsrc);
    theNACK.setLength(3);
    int nackLength = (theNACK.getLength()+1)*4;

    memcpy(buf, (uint8_t*)&theNACK, nackLength);
    return (len+nackLength); 
  }


} /* namespace erizo */
