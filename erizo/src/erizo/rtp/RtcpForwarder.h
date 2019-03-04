#ifndef ERIZO_SRC_ERIZO_RTP_RTCPFORWARDER_H_
#define ERIZO_SRC_ERIZO_RTP_RTCPFORWARDER_H_

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

class RtcpForwarder: public RtcpProcessor{
  DECLARE_LOGGER();

 public:
  RtcpForwarder(MediaSink* msink, MediaSource* msource, uint32_t max_video_bw = 300000);
  virtual ~RtcpForwarder() {}
  void addSourceSsrc(uint32_t ssrc) override;
  void setPublisherBW(uint32_t bandwidth) override;
  void analyzeSr(RtcpHeader* chead) override;
  int analyzeFeedback(char* buf, int len) override;
  void checkRtcpFb() override;
  std::pair<uint32_t, uint8_t> getLostPacketsInfo(uint32_t source_ssrc);
  void setLostPacketsInfo(uint32_t ssrc, uint32_t lost, uint8_t frac_lost) override;

 private:
  static const int RR_AUDIO_PERIOD = 2000;
  static const int RR_VIDEO_BASE = 800;
  static const int REMB_TIMEOUT = 1000;
  std::map<uint32_t, boost::shared_ptr<RtcpData>> rtcpData_;
  boost::mutex mapLock_;
  int addREMB(char* buf, int len, uint32_t bitrate);
  int addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc, uint32_t sinkSsrc);
  std::pair<uint32_t, uint8_t> initAndGetLostPacketsInfo(uint32_t source_ssrc);

  std::map<uint32_t, std::pair<uint32_t, uint8_t>> lost_packet_info_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTCPFORWARDER_H_
