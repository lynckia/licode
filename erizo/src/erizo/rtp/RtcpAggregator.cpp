/*
 * RtcpAggregator.cpp
 */

#include "rtp/RtcpAggregator.h"

#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <cstring>

#include "lib/Clock.h"
#include "lib/ClockUtils.h"

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(RtcpAggregator, "rtp.RtcpAggregator");

RtcpAggregator::RtcpAggregator(MediaSink* msink, MediaSource* msource, uint32_t max_video_bw)
    : RtcpProcessor(msink, msource, max_video_bw), defaultVideoBw_(max_video_bw / 2) {
  ELOG_DEBUG("Starting RtcpAggregator");
}

void RtcpAggregator::addSourceSsrc(uint32_t ssrc) {
  boost::mutex::scoped_lock mlock(mapLock_);
  if (rtcpData_.find(ssrc) == rtcpData_.end()) {
    this->rtcpData_[ssrc] = boost::shared_ptr<RtcpData>(new RtcpData());
    if (ssrc == this->rtcpSource_->getAudioSourceSSRC()) {
      ELOG_DEBUG("It is an audio SSRC %u", ssrc);
      this->rtcpData_[ssrc]->mediaType = AUDIO_TYPE;
    } else {
      ELOG_DEBUG("It is a video SSRC %u", ssrc);
      this->rtcpData_[ssrc]->mediaType = VIDEO_TYPE;
    }
  }
}

void RtcpAggregator::setPublisherBW(uint32_t bandwidth) {
  defaultVideoBw_ = (bandwidth*1.2) > max_video_bw_? max_video_bw_:(bandwidth*1.2);
}

void RtcpAggregator::analyzeSr(RtcpHeader* chead) {
  uint32_t recvSSRC = chead->getSSRC();
  // We try to add it just in case it is not there yet (otherwise its noop)
  this->addSourceSsrc(recvSSRC);

  boost::mutex::scoped_lock mlock(mapLock_);
  boost::shared_ptr<RtcpData> theData = rtcpData_[recvSSRC];
  boost::mutex::scoped_lock lock(theData->dataLock);

  uint64_t now = ClockUtils::timePointToMs(clock::now());
  uint32_t ntp;
  uint64_t theNTP = chead->getNtpTimestamp();
  ntp = (theNTP & (0xFFFFFFFF0000)) >> 16;
  theData->senderReports.push_back(boost::shared_ptr<SrDelayData>( new SrDelayData(recvSSRC, ntp, now)));
  // We only store the last 20 sr
  if (theData->senderReports.size() > 20) {
    theData->senderReports.pop_front();
  }
}
int RtcpAggregator::analyzeFeedback(char *buf, int len) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(buf);
  if (chead->isFeedback()) {
    if (chead->getBlockCount() == 0 && (chead->getLength()+1) * 4  == len) {
      ELOG_DEBUG("Ignoring empty RR");
      return 0;
    }
    uint32_t sourceSsrc = chead->getSourceSSRC();
    // We try to add it just in case it is not there yet (otherwise its noop)
    this->addSourceSsrc(sourceSsrc);

    boost::mutex::scoped_lock mlock(mapLock_);
    boost::shared_ptr<RtcpData> theData = rtcpData_[sourceSsrc];
    boost::mutex::scoped_lock lock(theData->dataLock);
    uint64_t nowms = ClockUtils::timePointToMs(clock::now());
    char* movingBuf = buf;
    int rtcpLength = 0;
    int totalLength = 0;
    int partNum = 0;
    uint16_t currentNackPos = 0;
    uint16_t blp = 0;
    uint32_t lostPacketSeq = 0;
    uint32_t delay = 0;
    uint32_t calculatedlsr, calculateLastSr, extendedSeqNo;

    do {
      movingBuf += rtcpLength;
      chead = reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength = (ntohs(chead->length) + 1) * 4;
      totalLength += rtcpLength;
      switch (chead->packettype) {
        case RTCP_SDES_PT:
          ELOG_DEBUG("SDES");
          break;
        case RTCP_BYE:
          ELOG_DEBUG("BYE");
          break;
        case RTCP_Receiver_PT:
          theData->rrsReceivedInPeriod++;
          if (rtcpSource_->isVideoSourceSSRC(chead->getSourceSSRC())) {
            ELOG_DEBUG("Analyzing Video RR: PacketLost %u, Ratio %u, partNum %d, blocks %d, sourceSSRC %u, ssrc %u",
                        chead->getLostPackets(), chead->getFractionLost(), partNum, chead->getBlockCount(),
                        chead->getSourceSSRC(), chead->getSSRC());
          } else {
            ELOG_DEBUG("Analyzing Audio RR: PacketLost %u, Ratio %u, partNum %d, blocks %d, sourceSSRC %u, ssrc %u",
                        chead->getLostPackets(), chead->getFractionLost(), partNum, chead->getBlockCount(),
                        chead->getSourceSSRC(), chead->getSSRC());
          }
          theData->ratioLost = theData->ratioLost > chead->getFractionLost() ?
                                            theData->ratioLost : chead->getFractionLost();
          theData->totalPacketsLost = theData->totalPacketsLost > chead->getLostPackets() ?
                                            theData->totalPacketsLost : chead->getLostPackets();
          extendedSeqNo = chead->getSeqnumCycles();
          extendedSeqNo = (extendedSeqNo << 16) + chead->getHighestSeqnum();
          if (extendedSeqNo > theData->extendedSeqNo) {
            theData->extendedSeqNo = extendedSeqNo;
            theData->highestSeqNumReceived = chead->getHighestSeqnum();
            theData->seqNumCycles = chead->getSeqnumCycles();
          }
          theData->jitter = theData->jitter > chead->getJitter()? theData->jitter: chead->getJitter();
          calculateLastSr = chead->getLastSr();
          calculatedlsr = (chead->getDelaySinceLastSr() * 1000) / 65536;
          for (std::list<boost::shared_ptr<SrDelayData>>::iterator it = theData->senderReports.begin();
                        it != theData->senderReports.end(); ++it) {
            if ((*it)->sr_ntp == calculateLastSr) {
              uint64_t sentts = (*it)->sr_send_time;
              delay = nowms - sentts - calculatedlsr;
            }
          }

          if (theData->lastSr == 0 || theData->lastDelay < delay) {
            ELOG_DEBUG("Recording DLSR %u, lastSR %u last delay %u, calculated delay %u for SSRC %u",
                        chead->getDelaySinceLastSr(), chead->getLastSr(), theData->lastDelay, delay, sourceSsrc);
            theData->lastSr = chead->getLastSr();
            theData->delaySinceLastSr = chead->getDelaySinceLastSr();
            theData->last_sr_updated = nowms;
            theData->lastDelay = delay;
          } else {
            // ELOG_DEBUG("Not recording delay %u, lastDelay %u", delay, theData->lastDelay);
          }
          break;
        case RTCP_RTP_Feedback_PT:
          {
            ELOG_DEBUG("RTP FB: Usually NACKs: %u, partNum %d", chead->getBlockCount(), partNum);
            ELOG_DEBUG("NACK PID %u BLP %u", chead->getNackPid(), chead->getNackBlp());
            // We analyze NACK to avoid sending repeated NACKs
            blp = chead->getNackBlp();
            theData->shouldSendNACK = false;
            std::pair<std::set<uint32_t>::iterator, bool> ret;
            ret = theData->nackedPackets_.insert(chead->getNackPid());
            if (ret.second) {
              ELOG_DEBUG("We received PID NACK for unacked packet %u", chead->getNackPid());
              theData->shouldSendNACK = true;
            } else {
              if (theData->nackedPackets_.size() >= MAP_NACK_SIZE) {
                while (theData->nackedPackets_.size() >= MAP_NACK_SIZE) {
                  theData->nackedPackets_.erase(theData->nackedPackets_.begin());
                }
              }
              ELOG_DEBUG("We received PID NACK for ALREADY acked packet %u", chead->getNackPid());
            }
            if (blp != 0) {
              for (int i = 0; i < 16; i++) {
                currentNackPos = blp & 0x0001;
                blp = blp >> 1;

                if (currentNackPos == 1) {
                  lostPacketSeq = chead->getNackPid() + 1 + i;
                  ret = theData->nackedPackets_.insert(lostPacketSeq);
                  if (ret.second) {
                    ELOG_DEBUG("We received NACK for unacked packet %u", lostPacketSeq);
                  } else {
                    ELOG_DEBUG("We received NACK for ALREADY acked packet %u", lostPacketSeq);
                  }
                  theData->shouldSendNACK |=ret.second;
                }
              }
            }
            if (theData->shouldSendNACK) {
              ELOG_DEBUG("Will send NACK");
              theData->nackSeqnum = chead->getNackPid();
              theData->nackBlp = chead->getNackBlp();
              theData->requestRr = true;
            } else {
              ELOG_DEBUG("I'm ignoring a NACK");
            }
          }
          break;
        case RTCP_PS_Feedback_PT:
          //            ELOG_DEBUG("RTCP PS FB TYPE: %u", chead->getBlockCount() );
          switch (chead->getBlockCount()) {
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
                char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
                if (!strncmp(uniqueId, "REMB", 4)) {
                  uint64_t bitrate = chead->getBrMantis() << chead->getBrExp();
                  ELOG_DEBUG("Received REMB %lu", bitrate);
                  if (bitrate < defaultVideoBw_) {
                    theData->reportedBandwidth = bitrate;
                    theData->shouldSendREMB = true;
                  } else {
                    theData->reportedBandwidth = defaultVideoBw_;
                  }
                } else {
                  ELOG_DEBUG("Unsupported AFB Packet not REMB")
                }
                break;
              }
            default:
              ELOG_WARN("Unsupported RTCP_PS FB TYPE %u", chead->getBlockCount());
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
  return 0;
}

void RtcpAggregator::checkRtcpFb() {
  boost::mutex::scoped_lock mlock(mapLock_);
  std::map<uint32_t, boost::shared_ptr<RtcpData>>::iterator it;
  for (it = rtcpData_.begin(); it != rtcpData_.end(); it++) {
    boost::shared_ptr<RtcpData> rtcpData = it->second;
    boost::mutex::scoped_lock lock(rtcpData->dataLock);
    uint32_t sourceSsrc = it->first;
    uint32_t sinkSsrc;
    uint64_t now = ClockUtils::timePointToMs(clock::now());

    unsigned int dt = now - rtcpData->last_rr_sent;
    unsigned int edlsr = now - rtcpData->last_sr_updated;
    unsigned int dt_scheduled = now - rtcpData->last_rr_was_scheduled;

    // Generate Full RTCP Packet
    if ((rtcpData->requestRr || dt >= rtcpData->nextPacketInMs ||rtcpData->shouldSendREMB) && rtcpData->lastSr > 0) {
      rtcpData->requestRr = false;
      RtcpHeader rtcpHead;
      rtcpHead.setPacketType(RTCP_Receiver_PT);
      if (sourceSsrc == rtcpSource_->getAudioSourceSSRC()) {
        sinkSsrc = rtcpSink_->getAudioSinkSSRC();
        ELOG_DEBUG("Sending Audio RR: PacketLost %u, Ratio %u, DLSR %u"
                   ", lastSR %u, DLSR Adjusted %u dt %u, rrsSinceLast %u highestSeqNum %u",
                  rtcpData->totalPacketsLost,
                  rtcpData->ratioLost,
                  rtcpData->delaySinceLastSr,
                  rtcpData->lastSr,
                  (rtcpData->delaySinceLastSr+edlsr),
                  dt,
                  rtcpData->rrsReceivedInPeriod,
                  rtcpData->highestSeqNumReceived);
        rtcpHead.setSSRC(rtcpSink_->getAudioSinkSSRC());
        rtcpHead.setSourceSSRC(rtcpSource_->getAudioSourceSSRC());
      } else {
        sinkSsrc = rtcpSink_->getVideoSinkSSRC();
        ELOG_DEBUG("Sending Video RR: SOurcessrc %u sinkSsrc %u PacketLost %u, Ratio %u, DLSR %u, lastSR %u"
                   ", DLSR Adjusted %u dt %u, rrsSinceLast %u highestSeqNum %u jitter %u",
                   sourceSsrc,
                   rtcpSink_->getVideoSinkSSRC(),
                   rtcpData->totalPacketsLost,
                   rtcpData->ratioLost,
                   rtcpData->delaySinceLastSr,
                   rtcpData->lastSr,
                   (rtcpData->delaySinceLastSr+edlsr),
                   dt, rtcpData->rrsReceivedInPeriod,
                   rtcpData->highestSeqNumReceived,
                   rtcpData->jitter);
        rtcpHead.setSSRC(rtcpSink_->getVideoSinkSSRC());
        rtcpHead.setSourceSSRC(rtcpSource_->getVideoSourceSSRC());
      }

      // rtcpHead.setFractionLost(rtcpData->ratioLost);
      // Calculate ratioLost
      uint32_t packetsReceivedinInterval = rtcpData->extendedSeqNo - rtcpData->prevExtendedSeqNo;
      uint32_t packetsLostInInterval = rtcpData->totalPacketsLost - rtcpData->prevTotalPacketsLost;
      double ratio = static_cast<double>(packetsLostInInterval/packetsReceivedinInterval);
      rtcpHead.setFractionLost(ratio*256);
      rtcpData->prevTotalPacketsLost = rtcpData->totalPacketsLost;
      rtcpData->prevExtendedSeqNo = rtcpData->extendedSeqNo;

      rtcpHead.setHighestSeqnum(rtcpData->highestSeqNumReceived);
      rtcpHead.setSeqnumCycles(rtcpData->seqNumCycles);
      rtcpHead.setLostPackets(rtcpData->totalPacketsLost);
      rtcpHead.setJitter(rtcpData->jitter);
      rtcpHead.setLastSr(rtcpData->lastSr);
      rtcpHead.setDelaySinceLastSr(rtcpData->delaySinceLastSr + ((edlsr*65536)/1000));
      rtcpHead.setLength(7);
      rtcpHead.setBlockCount(1);
      int length = (rtcpHead.getLength()+1)*4;
      memcpy(packet_, reinterpret_cast<uint8_t*>(&rtcpHead), length);
      if (rtcpData->shouldSendNACK) {
        ELOG_DEBUG("SEND NACK, SENDING with Seqno: %u", rtcpData->nackSeqnum);
        int theLen = this->addNACK(reinterpret_cast<char*>(packet_), length, rtcpData->nackSeqnum,
                        rtcpData->nackBlp, sourceSsrc, sinkSsrc);
        rtcpData->shouldSendNACK = false;
        rtcpData->nackSeqnum = 0;
        rtcpData->nackBlp = 0;
        length = theLen;
      }
      if (rtcpData->mediaType == VIDEO_TYPE) {
        unsigned int sincelastREMB = now - rtcpData->last_remb_sent;
        if (sincelastREMB > REMB_TIMEOUT) {
          // We dont have any more RRs, we follow what the publisher is doing to avoid congestion
          rtcpData->shouldSendREMB = true;
        }

        if (rtcpData->shouldSendREMB) {
          ELOG_DEBUG("Sending REMB, since last %u ms, sending with BW: %lu",
                      sincelastREMB, rtcpData->reportedBandwidth);
          int theLen = this->addREMB(reinterpret_cast<char*>(packet_), length, rtcpData->reportedBandwidth);
          rtcpData->shouldSendREMB = false;
          rtcpData->last_remb_sent = now;
          length = theLen;
        }
      }
      if  (rtcpSource_->isVideoSourceSSRC(sourceSsrc)) {
        rtcpSink_->deliverVideoData(std::make_shared<DataPacket>(0, reinterpret_cast<char*>(packet_),
              length, VIDEO_PACKET));
      } else {
        rtcpSink_->deliverAudioData(std::make_shared<DataPacket>(0, reinterpret_cast<char*>(packet_),
              length, AUDIO_PACKET));
      }
      rtcpData->last_rr_sent = now;
      if (dt_scheduled > rtcpData->nextPacketInMs) {  // Every scheduled packet we reset
        rtcpData->shouldReset = true;
      }
      rtcpData->last_rr_was_scheduled = now;
      // schedule next packet
      std::string thread_id = boost::lexical_cast<std::string>(boost::this_thread::get_id());
      unsigned int thread_number = 0;
      sscanf(thread_id.c_str(), "%x", &thread_number);

      float random = (rand_r(&thread_number) % 100 + 50) / 100.0;
      if (rtcpData->mediaType == AUDIO_TYPE) {
        rtcpData->nextPacketInMs = RTCP_AUDIO_INTERVAL*random;
        ELOG_DEBUG("Scheduled next Audio RR in %u ms", rtcpData->nextPacketInMs);
      } else {
        rtcpData->nextPacketInMs = (RTCP_VIDEO_INTERVAL)*random;
        ELOG_DEBUG("Scheduled next Video RR in %u ms", rtcpData->nextPacketInMs);
      }

      if (rtcpData->shouldReset) {
        this->resetData(rtcpData, this->defaultVideoBw_);
      }
    }
    if (rtcpData->shouldSendPli) {
      rtcpSource_->sendPLI();
      rtcpData->shouldSendPli = false;
    }
  }
}

int RtcpAggregator::addREMB(char* buf, int len, uint32_t bitrate) {
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
  theREMB.setREMBFeedSSRC(0, rtcpSource_->getVideoSourceSSRC());
  int rembLength = (theREMB.getLength()+1)*4;

  memcpy(buf, reinterpret_cast<uint8_t*>(&theREMB), rembLength);
  return (len + rembLength);
}

int RtcpAggregator::addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc, uint32_t sinkSsrc) {
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

  memcpy(buf, reinterpret_cast<uint8_t*>(&theNACK), nackLength);
  return (len + nackLength);
}

void RtcpAggregator::resetData(boost::shared_ptr<RtcpData> data, uint32_t bandwidth) {
  data->ratioLost = 0;
  data->requestRr = false;
  data->shouldReset = false;
  data->jitter = 0;
  data->rrsReceivedInPeriod = 0;
  if (data->reportedBandwidth > bandwidth) {
    data->reportedBandwidth = bandwidth;
  }
  data->lastDelay = data->lastDelay*0.6;
}
}  // namespace erizo
