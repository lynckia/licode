/*
 * Stats.cpp
 *
 */

#include <sstream>
#include <string>

#include "Stats.h"
#include "WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

  DEFINE_LOGGER(Stats, "Stats");

  Stats::Stats() : latest_total_bitrate_{0} {
    ELOG_DEBUG("Constructor Stats");
    theListener_ = NULL;
    bitrate_calculation_start_ = clock::now();
  }

  Stats::~Stats() {
    ELOG_DEBUG("Destructor Stats");
  }

  void Stats::setVideoSourceSSRC(uint32_t ssrc) {
    video_source_ssrc_ = ssrc;
  }
  void Stats::setVideoSinkSSRC(uint32_t ssrc) {
    video_sink_ssrc_ = ssrc;
  }
  void Stats::setAudioSourceSSRC(uint32_t ssrc) {
    audio_source_ssrc_ = ssrc;
  }
  void Stats::setAudioSinkSSRC(uint32_t ssrc) {
    audio_sink_ssrc_ = ssrc;
  }

  void Stats::processRtpPacket(char* buf, int len) {
    boost::recursive_mutex::scoped_lock lock(mapMutex_);
    RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);
    uint32_t ssrc = head->getSSRC();
    if (!isSinkSSRC(ssrc) && !isSourceSSRC(ssrc)) {
      ELOG_DEBUG("message: Unknown SSRC in processRtpPacket, ssrc: %u, PT: %u", ssrc, head->getPayloadType());
    }
    if (bitrate_bytes_map.find(ssrc) == bitrate_bytes_map.end()) {
      bitrate_bytes_map[ssrc] = 0;
    }
    bitrate_bytes_map[ssrc] += len;
    time_point nowms = clock::now();
    time_point start = bitrate_calculation_start_;
    duration delay = nowms - start;
    uint32_t total_bitrate = 0;
    if (delay > kBitrateStatsPeriod) {
      for (auto &bytes_pair : bitrate_bytes_map) {
        uint32_t calculated_value = ((8 * bytes_pair.second * 1000) / ClockUtils::durationToMs(delay));  // in bps
        setBitrateCalculated(calculated_value, bytes_pair.first);
        total_bitrate += calculated_value;
        bytes_pair.second = 0;
      }
      bitrate_calculation_start_ = clock::now();
      latest_total_bitrate_ = total_bitrate;
    }
  }

  void Stats::processRtcpPacket(char* buf, int length) {
    boost::recursive_mutex::scoped_lock lock(mapMutex_);
    char* movingBuf = buf;
    int rtcpLength = 0;
    int totalLength = 0;
    uint32_t ssrc = 0;

    RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(movingBuf);
    if (chead->isFeedback()) {
      ssrc = chead->getSourceSSRC();
      if (!isSinkSSRC(ssrc)) {
        ELOG_DEBUG("message: Unknown SSRC in processRtcpPacket, ssrc %u, PT %u", ssrc, chead->getPacketType());
        return;
      }
    } else {
      ssrc = chead->getSSRC();
          if (!isSourceSSRC(ssrc)) {
            ELOG_DEBUG("message: Unknown SSRC in processRtcpPacket, ssrc %u, PT %u", ssrc, chead->getPacketType());
            return;
          }
    }
    ELOG_DEBUG("RTCP packet received, type: %u, size: %u, packetLength: %u", chead->getPacketType(),
         ((ntohs(chead->length) + 1) * 4), length);
    do {
      movingBuf += rtcpLength;
      RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength = (ntohs(chead->length) + 1) * 4;
      totalLength += rtcpLength;
      ELOG_DEBUG("RTCP SubPacket: PT %d, SSRC %u, sourceSSRC %u, block count %d",
          chead->packettype, chead->getSSRC(), chead->getSourceSSRC(), chead->getBlockCount());
      switch (chead->packettype) {
        case RTCP_SDES_PT:
          ELOG_DEBUG("SDES");
          break;
        case RTCP_BYE:
          ELOG_DEBUG("RTCP BYE");
          break;
        case RTCP_Receiver_PT:
          setFractionLost(chead->getFractionLost(), ssrc);
          setPacketsLost(chead->getLostPackets(), ssrc);
          setJitter(chead->getJitter(), ssrc);
          setSourceSSRC(ssrc, ssrc);
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
          switch (chead->getBlockCount()) {
            case RTCP_PLI_FMT:
              ELOG_DEBUG("PLI Packet, SSRC %u, sourceSSRC %u", chead->getSSRC(), chead->getSourceSSRC());
              accountPLIMessage(ssrc);
              break;
            case RTCP_SLI_FMT:
              ELOG_DEBUG("SLI Message");
              accountSLIMessage(ssrc);
              break;
            case RTCP_FIR_FMT:
              ELOG_DEBUG("FIR Packet, SSRC %u, sourceSSRC %u", chead->getSSRC(), chead->getSourceSSRC());
              accountFIRMessage(ssrc);
              break;
            case RTCP_AFB:
              {
                ELOG_DEBUG("REMB Packet, SSRC %u, sourceSSRC %u", chead->getSSRC(), chead->getSourceSSRC());
                char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
                if (!strncmp(uniqueId, "REMB", 4)) {
                  uint64_t bitrate = chead->getREMBBitRate();
                  // ELOG_DEBUG("REMB Packet numSSRC %u mantissa %u exp %u, tot %lu bps",
                  //             chead->getREMBNumSSRC(), chead->getBrMantis(), chead->getBrExp(), bitrate);
                  setBandwidth(bitrate, ssrc);
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
    } while (totalLength < length);
    sendStats();
  }

  std::string Stats::getStats() {
    boost::recursive_mutex::scoped_lock lock(mapMutex_);
    std::ostringstream theString;
    theString << "[";
    for (fullStatsMap_t::iterator itssrc = ssrc_stats_.begin(); itssrc != ssrc_stats_.end();) {
      uint32_t currentSSRC = itssrc->first;
      theString << "{\"ssrc\":\"" << currentSSRC << "\",\n";
      if (currentSSRC == video_source_ssrc_ || currentSSRC == video_sink_ssrc_) {
        theString << "\"type\":\"" << "video\",\n";
      } else if (currentSSRC == audio_source_ssrc_ || currentSSRC == audio_sink_ssrc_) {
        theString << "\"type\":\"" << "audio\",\n";
      }
      for (singleSSRCstatsMap_t::iterator it = ssrc_stats_[currentSSRC].begin();
           it != ssrc_stats_[currentSSRC].end();) {
        theString << "\"" << it->first << "\":\"" << it->second << "\"";
        if (++it != ssrc_stats_[currentSSRC].end()) {
          theString << ",\n";
        }
      }
      theString << "}";
      if (++itssrc != ssrc_stats_.end()) {
        theString << ",";
      }
    }
    theString << "]";
    return theString.str();
  }

  void Stats::setSlideShowMode(bool is_active, uint32_t ssrc) {
    boost::recursive_mutex::scoped_lock lock(mapMutex_);
    setErizoSlideShow(is_active, ssrc);
  }

  void Stats::setMute(bool is_active, uint32_t ssrc) {
    boost::recursive_mutex::scoped_lock lock(mapMutex_);
    setErizoMute(is_active, ssrc);
  }

  void Stats::setEstimatedBandwidth(uint32_t bandwidth, uint32_t ssrc) {
    boost::recursive_mutex::scoped_lock lock(mapMutex_);
    setErizoEstimatedBandwidth(bandwidth, ssrc);
  }

  void Stats::sendStats() {
    if (theListener_ != NULL)
      theListener_->notifyStats(this->getStats());
  }
}  // namespace erizo
