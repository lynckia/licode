#ifndef RTCPPROCESSOR_H_
#define RTCPPROCESSOR_H_

#include <map>
#include <list>
#include <boost/shared_ptr.hpp>

#include "logger.h"
#include "MediaDefinitions.h"
#include "SdpInfo.h"
#include "rtp/RtpHeaders.h"

namespace erizo {
 
  // Forward declaration

  class SrData {
    public:
      uint32_t srNtp;
      struct timeval timestamp;

      SrData() {
        srNtp = 0;
        timestamp = (struct timeval){0, 0} ;
      };
      SrData(uint32_t srNTP, struct timeval theTimestamp){
        this->srNtp = srNTP;
        this->timestamp = theTimestamp;
      }
  };
  
  class RtcpData {
  // lost packets - list and length
  public:
    uint32_t *nackList;
    int nackLen;

    // current values - tracks packet lost for fraction calculation
    uint32_t packetCount;
    uint16_t rrsReceivedInPeriod;

    uint32_t ssrc;
    uint32_t totalPacketsLost;
    uint32_t ratioLost:8;
    uint32_t highestSeqNumReceived;
    uint32_t lastSr;
    uint64_t reportedBandwidth;
    uint32_t delaySinceLastSr;
    
    uint32_t lastDelay;

    uint32_t jitter;
    // last SR field
    uint32_t lastSrTimestamp;
    // required to properly calculate DLSR
    uint16_t nackSeqnum;
    uint16_t nackBlp;

    // time based data flow limits
    struct timeval lastUpdated, lastSent, lastSrUpdated;
    struct timeval lastREMBSent, lastPliSent;
    struct timeval lastSrReception;
    // to prevent sending too many reports, track time of last
    struct timeval lastRrSent;
    
    bool shouldSendPli;
    bool shouldSendREMB;
    bool shouldSendNACK;
    // flag to send receiver report
    bool requestRr;
    bool hasSentFirstRr;

    MediaType mediaType;

    std::list<boost::shared_ptr<SrData>> senderReports;

    void reset(){
      ratioLost = 0;
      requestRr = false;
      jitter = 0;
      rrsReceivedInPeriod = 0;
      reportedBandwidth = 0;
      lastDelay = lastDelay*0.8;
    }

    RtcpData(){
      packetCount = 0;        
      rrsReceivedInPeriod = 0;
      totalPacketsLost = 0;
      ratioLost = 0;
      highestSeqNumReceived = 0;
      lastSr = 0;
      reportedBandwidth = 0;
      delaySinceLastSr = 0;
      jitter = 0;
      lastSrTimestamp = 0;
      requestRr = false;
      hasSentFirstRr = false;
      lastDelay = 0;
     
      shouldSendPli = false;
      shouldSendREMB = false;
      shouldSendNACK = false;
      nackSeqnum = 0;
      nackBlp = 0;
      lastRrSent = (struct timeval){0, 0};
      lastPliSent = (struct timeval){0, 0};
      lastREMBSent = (struct timeval){0, 0};
      lastSrReception = (struct timeval){0, 0};
    }

    // lock for any blocking data change
    boost::mutex dataLock;
};

class RtcpProcessor{
	DECLARE_LOGGER();
  
  public:
    RtcpProcessor(MediaSink* msink, MediaSource* msource, int defaultBw = 300000);
    virtual ~RtcpProcessor(){
    };
    void addSourceSsrc(uint32_t ssrc);
    void setVideoBW(uint32_t bandwidth);
    void analyzeSr(RtcpHeader* chead);
    void analyzeFeedback(char* buf, int len);
    void checkRtcpFb();
    int addREMB(char* buf, int len, uint32_t bitrate);
    int addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc);

  private:
    static const int RTCP_PERIOD = 200;
    static const int PLI_THRESHOLD = 50;
    static const int REMB_TIMEOUT = 2000;
    static const uint64_t NTPTOMSCONV = 4294967296;
    std::map<uint32_t, boost::shared_ptr<RtcpData>> rtcpData_;
    MediaSink* rtcpSink_;  // The sink to send RRs
    MediaSource* rtcpSource_; // The source of SRs
    uint32_t defaultBw_;

};

} /* namespace erizo */

#endif /* RTCPPROCESSOR_H_ */
