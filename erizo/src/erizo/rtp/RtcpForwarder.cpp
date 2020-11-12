/*
 * RtcpForwarder.cpp
 */
#include "rtp/RtcpForwarder.h"

#include <string>
#include <cstring>

#include "lib/Clock.h"
#include "lib/ClockUtils.h"

using std::memcpy;

namespace erizo {
DEFINE_LOGGER(RtcpForwarder, "rtp.RtcpForwarder");

RtcpForwarder::RtcpForwarder(MediaSink* msink, MediaSource* msource, uint32_t max_video_bw)
  : RtcpProcessor(msink, msource, max_video_bw) {
    ELOG_DEBUG("Starting RtcpForwarder");
  }

void RtcpForwarder::addSourceSsrc(uint32_t ssrc) {
  boost::mutex::scoped_lock mlock(mapLock_);
  if (rtcpData_.find(ssrc) == rtcpData_.end()) {
    this->rtcpData_[ssrc] = boost::shared_ptr<RtcpData>(new RtcpData());
    if (ssrc == this->rtcpSource_->getAudioSourceSSRC()) {
      ELOG_DEBUG("Adding new Audio source SSRC %u", ssrc);
      this->rtcpData_[ssrc]->mediaType = AUDIO_TYPE;
    } else {
      ELOG_DEBUG("Adding new Video source SSRC %u", ssrc);
      this->rtcpData_[ssrc]->mediaType = VIDEO_TYPE;
    }
  }
}

void RtcpForwarder::setPublisherBW(uint32_t bandwidth) {
}

void RtcpForwarder::analyzeSr(RtcpHeader* chead) {
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
int RtcpForwarder::analyzeFeedback(char *buf, int len) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(buf);
  if (chead->isFeedback()) {
    if (chead->getBlockCount() == 0 && (chead->getLength() + 1) * 4  == len) {
      ELOG_DEBUG("Ignoring empty RR");
      return 0;
    }
    uint32_t sourceSsrc = chead->getSourceSSRC();
    // We try to add it just in case it is not there yet (otherwise its noop)
    this->addSourceSsrc(sourceSsrc);

    char* movingBuf = buf;
    int rtcpLength = 0;
    int totalLength = 0;
    int currentBlock = 0;

    do {
      movingBuf+=rtcpLength;
      chead = reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength = (ntohs(chead->length) + 1) * 4;
      totalLength += rtcpLength;
      switch (chead->packettype) {
        case RTCP_SDES_PT:
          ELOG_DEBUG("SDES");
          break;
        case RTCP_BYE:
          ELOG_DEBUG("Dropping BYE packet");
          return 0;
          break;
        case RTCP_Receiver_PT:
          if (rtcpSource_->isVideoSourceSSRC(chead->getSourceSSRC())) {
            ELOG_DEBUG("Analyzing Video RR: PacketLost %u, Ratio %u, currentBlock %d, blocks %d"
                       ", sourceSSRC %u, ssrc %u changed to %u",
                chead->getLostPackets(),
                chead->getFractionLost(),
                currentBlock,
                chead->getBlockCount(),
                chead->getSourceSSRC(),
                chead->getSSRC(),
                rtcpSink_->getVideoSinkSSRC());
            chead->setSSRC(rtcpSink_->getVideoSinkSSRC());
          } else {
            ELOG_DEBUG("Analyzing Audio RR: PacketLost %u, Ratio %u, currentBlock %d, blocks %d"
                       ", sourceSSRC %u, ssrc %u changed to %u",
                chead->getLostPackets(),
                chead->getFractionLost(),
                currentBlock,
                chead->getBlockCount(),
                chead->getSourceSSRC(),
                chead->getSSRC(),
                rtcpSink_->getAudioSinkSSRC());
            chead->setSSRC(rtcpSink_->getAudioSinkSSRC());
          }
          break;
        case RTCP_RTP_Feedback_PT:
          ELOG_DEBUG("RTP FB: Usually NACKs: %u, currentBlock %d", chead->getBlockCount(), currentBlock);
          ELOG_DEBUG("NACK PID %u BLP %u", chead->getNackPid(), chead->getNackBlp());
          // We analyze NACK to avoid sending repeated NACKs
          break;
        case RTCP_PS_Feedback_PT:
          //            ELOG_DEBUG("RTCP PS FB TYPE: %u", chead->getBlockCount() );
          switch (chead->getBlockCount()) {
            case RTCP_PLI_FMT:
              ELOG_DEBUG("PLI Message, currentBlock %d", currentBlock);
              // 1: PLI, 4: FIR
              break;
            case RTCP_SLI_FMT:
              ELOG_WARN("SLI Message");
              break;
            case RTCP_FIR_FMT:
              ELOG_WARN("FIR Message");
              break;
            case RTCP_AFB:
              {
                char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
                if (!strncmp(uniqueId, "REMB", 4)) {
                  uint64_t bitrate = chead->getBrMantis() << chead->getBrExp();
                  uint64_t cappedBitrate = 0;
                  cappedBitrate = bitrate < max_video_bw_? bitrate: max_video_bw_;
                  if (bitrate < max_video_bw_) {
                    cappedBitrate = bitrate;
                  } else {
                    cappedBitrate = max_video_bw_;
                  }
                  ELOG_DEBUG("Received REMB %lu, partnum %u, cappedBitrate %lu",
                              bitrate, currentBlock, cappedBitrate);
                  chead->setREMBBitRate(cappedBitrate);
                } else {
                  ELOG_WARN("Unsupported AFB Packet not REMB")
                }
                break;
              }
            default:
              ELOG_WARN("Unsupported RTCP_PS FB TYPE %u", chead->getBlockCount());
              break;
          }
          break;
        default:
          ELOG_WARN("Unknown RTCP Packet, %d", chead->packettype);
          break;
      }
      currentBlock++;
    } while (totalLength < len);
    return len;
  }
  return 0;
}

void RtcpForwarder::checkRtcpFb() {
}

int RtcpForwarder::addREMB(char* buf, int len, uint32_t bitrate) {
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
  return (len+rembLength);
}

int RtcpForwarder::addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc, uint32_t sinkSsrc) {
  buf += len;
  ELOG_DEBUG("Setting PID %u, BLP %u", seqNum , blp);
  RtcpHeader theNACK;
  theNACK.setPacketType(RTCP_RTP_Feedback_PT);
  theNACK.setBlockCount(1);
  theNACK.setNackPid(seqNum);
  theNACK.setNackBlp(blp);
  theNACK.setSSRC(sinkSsrc);
  theNACK.setSourceSSRC(sourceSsrc);
  theNACK.setLength(3);
  int nackLength = (theNACK.getLength() + 1) * 4;

  memcpy(buf, reinterpret_cast<uint8_t*>(&theNACK), nackLength);
  return (len + nackLength);
}
}  // namespace erizo
