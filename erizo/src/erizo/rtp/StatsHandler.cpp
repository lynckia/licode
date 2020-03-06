#include "rtp/StatsHandler.h"

#include <string>

#include "./MediaDefinitions.h"
#include "./MediaStream.h"



namespace erizo {

DEFINE_LOGGER(StatsCalculator, "rtp.StatsCalculator");
DEFINE_LOGGER(IncomingStatsHandler, "rtp.IncomingStatsHandler");
DEFINE_LOGGER(OutgoingStatsHandler, "rtp.OutgoingStatsHandler");

void StatsCalculator::update(MediaStream *stream, std::shared_ptr<Stats> stats) {
  if (!stream_) {
    stream_ = stream;
    stats_ = stats;
    if (!getStatsInfo().hasChild("total")) {
      getStatsInfo()["total"].insertStat("bitrateCalculated", MovingIntervalRateStat{kRateStatIntervalSize,
        kRateStatIntervals, 8.});
    }
  }
}

void StatsCalculator::processPacket(std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    processRtcpPacket(packet);
  } else {
    processRtpPacket(packet);
  }
}

void StatsCalculator::processRtpPacket(std::shared_ptr<DataPacket> packet) {
  char* buf = packet->data;
  int len = packet->length;
  RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);
  uint32_t ssrc = head->getSSRC();
  if (!stream_->isSinkSSRC(ssrc) && !stream_->isSourceSSRC(ssrc)) {
    ELOG_DEBUG("message: Unknown SSRC in processRtpPacket, ssrc: %u, PT: %u", ssrc, head->getPayloadType());
    return;
  }
  if (!getStatsInfo()[ssrc].hasChild("bitrateCalculated")) {
    if (stream_->isVideoSourceSSRC(ssrc) || stream_->isVideoSinkSSRC(ssrc)) {
      getStatsInfo()[ssrc].insertStat("type", StringStat{"video"});
    } else if (stream_->isAudioSourceSSRC(ssrc) || stream_->isAudioSinkSSRC(ssrc)) {
      getStatsInfo()[ssrc].insertStat("type", StringStat{"audio"});
    }
    getStatsInfo()[ssrc].insertStat("bitrateCalculated", MovingIntervalRateStat{kRateStatIntervalSize,
        kRateStatIntervals, 8.});
    getStatsInfo()[ssrc].insertStat("mediaBitrateCalculated", MovingIntervalRateStat{kRateStatIntervalSize,
        kRateStatIntervals, 8.});
  }
  getStatsInfo()[ssrc]["bitrateCalculated"] += len;
  getStatsInfo()["total"]["bitrateCalculated"] += len;
  if (!packet->is_padding) {
    getStatsInfo()[ssrc]["mediaBitrateCalculated"] += len;
  }
  if (packet->type == VIDEO_PACKET) {
    stream_->setVideoBitrate(getStatsInfo()[ssrc]["mediaBitrateCalculated"].value());
    if (packet->is_keyframe) {
      incrStat(ssrc, "keyFrames");
    }
  }
}

void StatsCalculator::incrStat(uint32_t ssrc, std::string stat) {
  if (!getStatsInfo()[ssrc].hasChild(stat)) {
    getStatsInfo()[ssrc].insertStat(stat, CumulativeStat{1});
    return;
  }
  getStatsInfo()[ssrc][stat]++;
}

void StatsCalculator::processRtcpPacket(std::shared_ptr<DataPacket> packet) {
  char* buf = packet->data;
  int len = packet->length;

  char* movingBuf = buf;
  int rtcpLength = 0;
  int totalLength = 0;
  uint32_t ssrc = 0;

  bool is_feedback_on_publisher = false;

  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(movingBuf);
  if (chead->isFeedback()) {
    ssrc = chead->getSourceSSRC();
    if (stream_->isPublisher()) {
      is_feedback_on_publisher = true;
    }
  } else {
    ssrc = chead->getSSRC();
    if (!stream_->isSourceSSRC(ssrc)) {
      return;
    }
  }

  ELOG_DEBUG("RTCP packet received, type: %u, size: %u, packetLength: %u", chead->getPacketType(),
       ((ntohs(chead->length) + 1) * 4), len);
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
        if (is_feedback_on_publisher) {
          break;
        }
        ELOG_DEBUG("RTP RR: Fraction Lost %u, packetsLost %u", chead->getFractionLost(), chead->getLostPackets());
        getStatsInfo()[ssrc].insertStat("fractionLost", CumulativeStat{chead->getFractionLost()});
        getStatsInfo()[ssrc].insertStat("packetsLost", CumulativeStat{chead->getLostPackets()});
        getStatsInfo()[ssrc].insertStat("jitter", CumulativeStat{chead->getJitter()});
        getStatsInfo()[ssrc].insertStat("sourceSsrc", CumulativeStat{ssrc});
        break;
      case RTCP_Sender_PT:
        ELOG_DEBUG("RTP SR: Packets Sent %u, Octets Sent %u", chead->getPacketsSent(), chead->getOctetsSent());
        getStatsInfo()[ssrc].insertStat("packetsSent", CumulativeStat{chead->getPacketsSent()});
        getStatsInfo()[ssrc].insertStat("bytesSent", CumulativeStat{chead->getOctetsSent()});
        getStatsInfo()[ssrc].insertStat("srTimestamp", CumulativeStat{chead->getTimestamp()});
        getStatsInfo()[ssrc].insertStat("srNtp", CumulativeStat{chead->getNtpTimestamp()});
        break;
      case RTCP_RTP_Feedback_PT:
        ELOG_DEBUG("RTP FB: Usually NACKs: %u", chead->getBlockCount());
        ELOG_DEBUG("PID %u BLP %u", chead->getNackPid(), chead->getNackBlp());
        incrStat(ssrc, "NACK");
        break;
      case RTCP_PS_Feedback_PT:
        ELOG_DEBUG("RTCP PS FB TYPE: %u", chead->getBlockCount() );
        switch (chead->getBlockCount()) {
          case RTCP_PLI_FMT:
            ELOG_DEBUG("PLI Packet, SSRC %u, sourceSSRC %u", chead->getSSRC(), chead->getSourceSSRC());
            incrStat(ssrc, "PLI");
            break;
          case RTCP_SLI_FMT:
            ELOG_DEBUG("SLI Message");
            incrStat(ssrc, "SLI");
            break;
          case RTCP_FIR_FMT:
            ELOG_DEBUG("FIR Packet, SSRC %u, sourceSSRC %u", chead->getSSRC(), chead->getSourceSSRC());
            incrStat(ssrc, "FIR");
            break;
          case RTCP_AFB:
            {
              if (is_feedback_on_publisher) {
                break;
              }
              ssrc = chead->getREMBFeedSSRC(0);
              ELOG_DEBUG("REMB Packet, SSRC %u, sourceSSRC %u", chead->getSSRC(), chead->getSourceSSRC());
              char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
              if (!strncmp(uniqueId, "REMB", 4)) {
                uint64_t bitrate = chead->getREMBBitRate();
                // ELOG_DEBUG("REMB Packet numSSRC %u mantissa %u exp %u, tot %lu bps",
                //             chead->getREMBNumSSRC(), chead->getBrMantis(), chead->getBrExp(), bitrate);
                getStatsInfo()[ssrc].insertStat("bandwidth", CumulativeStat{bitrate});
                getStatsInfo()["total"].insertStat("senderBitrateEstimation", CumulativeStat{bitrate});
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
  } while (totalLength < len);
  notifyStats();
}

IncomingStatsHandler::IncomingStatsHandler() : stream_{nullptr} {}

void IncomingStatsHandler::enable() {}

void IncomingStatsHandler::disable() {}

void IncomingStatsHandler::notifyUpdate() {
  if (stream_) {
    return;
  }
  auto pipeline = getContext()->getPipelineShared();
  update(pipeline->getService<MediaStream>().get(),
             pipeline->getService<Stats>());
}

void IncomingStatsHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  processPacket(packet);
  ctx->fireRead(std::move(packet));
}

OutgoingStatsHandler::OutgoingStatsHandler() : stream_{nullptr} {}

void OutgoingStatsHandler::enable() {}

void OutgoingStatsHandler::disable() {}

void OutgoingStatsHandler::notifyUpdate() {
  if (stream_) {
    return;
  }
  auto pipeline = getContext()->getPipelineShared();
  update(pipeline->getService<MediaStream>().get(),
             pipeline->getService<Stats>());
}

void OutgoingStatsHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  processPacket(packet);
  ctx->fireWrite(std::move(packet));
}

}  // namespace erizo
