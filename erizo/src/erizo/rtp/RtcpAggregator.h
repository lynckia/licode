#ifndef ERIZO_SRC_ERIZO_RTP_RTCPAGGREGATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPAGGREGATOR_H_

#include <boost/shared_ptr.hpp>

#include <map>
#include <list>
#include <set>

#include "./logger.h"
#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RtcpProcessor.h"

namespace erizo {
  static const int MAP_NACK_SIZE = 50;

class RtcpAggregator: public RtcpProcessor{
  DECLARE_LOGGER();

 public:
  RtcpAggregator(MediaSink* msink, MediaSource* msource, uint32_t max_video_bw = 300000);
  virtual ~RtcpAggregator() {}
  void addSourceSsrc(uint32_t ssrc);
  void setPublisherBW(uint32_t bandwidth);
  void analyzeSr(RtcpHeader* chead);
  int analyzeFeedback(char* buf, int len);
  void checkRtcpFb();

 private:
  static const int REMB_TIMEOUT = 1000;
  static const uint64_t NTPTOMSCONV = 4294967296;
  std::map<uint32_t, boost::shared_ptr<RtcpData>> rtcpData_;
  boost::mutex mapLock_;
  uint32_t defaultVideoBw_;
  uint8_t packet_[128];
  int addREMB(char* buf, int len, uint32_t bitrate);
  int addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc, uint32_t sinkSsrc);
  void resetData(boost::shared_ptr<RtcpData> data, uint32_t bandwidth);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPAGGREGATOR_H_
