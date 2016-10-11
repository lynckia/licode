#ifndef ERIZO_SRC_ERIZO_RTP_RTCPPROCESSOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPPROCESSOR_H_

#include <boost/shared_ptr.hpp>

#include <map>
#include <list>
#include <set>

#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "rtp/RtpHeaders.h"

namespace erizo {

class SrData {
 public:
  uint32_t srNtp;
  struct timeval timestamp;

  SrData() {
    srNtp = 0;
    timestamp = (struct timeval) {0, 0};
  }
  SrData(uint32_t srNTP, struct timeval theTimestamp) {
    this->srNtp = srNTP;
    this->timestamp = theTimestamp;
  }
};

class RtcpData {
// lost packets - list and length
 public:
  // current values - tracks packet lost for fraction calculation
  uint16_t rrsReceivedInPeriod;

  uint32_t ssrc;
  uint32_t totalPacketsLost;
  uint32_t prevTotalPacketsLost;
  uint32_t ratioLost:8;
  uint16_t highestSeqNumReceived;
  uint16_t seqNumCycles;
  uint32_t extendedSeqNo;
  uint32_t prevExtendedSeqNo;
  uint32_t lastSr;
  uint64_t reportedBandwidth;
  uint32_t maxBandwidth;
  uint32_t delaySinceLastSr;

  uint32_t nextPacketInMs;

  uint32_t lastDelay;

  uint32_t jitter;
  // last SR field
  uint32_t lastSrTimestamp;
  // required to properly calculate DLSR
  uint16_t nackSeqnum;
  uint16_t nackBlp;

  // time based data flow limits
  struct timeval lastSrUpdated, lastREMBSent;
  struct timeval lastSrReception, lastRrWasScheduled;
  // to prevent sending too many reports, track time of last
  struct timeval lastRrSent;

  bool shouldSendPli;
  bool shouldSendREMB;
  bool shouldSendNACK;
  // flag to send receiver report
  bool requestRr;
  bool shouldReset;

  MediaType mediaType;

  std::list<boost::shared_ptr<SrData>> senderReports;
  std::set<uint32_t> nackedPackets_;

  RtcpData() {
    nextPacketInMs = 0;
    rrsReceivedInPeriod = 0;
    totalPacketsLost = 0;
    prevTotalPacketsLost = 0;
    ratioLost = 0;
    highestSeqNumReceived = 0;
    seqNumCycles = 0;
    extendedSeqNo = 0;
    prevExtendedSeqNo = 0;
    lastSr = 0;
    reportedBandwidth = 0;
    delaySinceLastSr = 0;
    jitter = 0;
    lastSrTimestamp = 0;
    requestRr = false;
    lastDelay = 0;

    shouldSendPli = false;
    shouldSendREMB = false;
    shouldSendNACK = false;
    shouldReset = false;
    nackSeqnum = 0;
    nackBlp = 0;
    lastRrSent = (struct timeval){0, 0};
    lastREMBSent = (struct timeval){0, 0};
    lastSrReception = (struct timeval){0, 0};
    lastRrWasScheduled = (struct timeval){0, 0};
  }

  // lock for any blocking data change
  boost::mutex dataLock;
};

class RtcpProcessor{
 public:
  RtcpProcessor(MediaSink* msink, MediaSource* msource, uint32_t maxVideoBw = 300000):
    rtcpSink_(msink), rtcpSource_(msource) {}
  virtual ~RtcpProcessor() {}
  virtual void addSourceSsrc(uint32_t ssrc) = 0;
  virtual void setMaxVideoBW(uint32_t bandwidth) = 0;
  virtual void setPublisherBW(uint32_t bandwidth) = 0;
  virtual void analyzeSr(RtcpHeader* chead) = 0;
  virtual int analyzeFeedback(char* buf, int len) = 0;
  virtual void checkRtcpFb() = 0;

 protected:
  MediaSink* rtcpSink_;  // The sink to send RRs
  MediaSource* rtcpSource_;  // The source of SRs
  uint32_t maxVideoBw_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPPROCESSOR_H_
