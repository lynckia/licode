#include "./MediaDefinitions.h"
#include "rtp/SenderBandwidthEstimationHandler.h"

namespace erizo {

DEFINE_LOGGER(SenderBandwidthEstimationHandler, "rtp.SenderBandwidthEstimationHandler");

SenderBandwidthEstimationHandler::SenderBandwidthEstimationHandler() :
  connection_{nullptr}, initialized_{false}, enabled_{true}, packets_sent_{0},
  sender_bwe_{new SendSideBandwidthEstimation()} {};

SenderBandwidthEstimationHandler::SenderBandwidthEstimationHandler(const SenderBandwidthEstimationHandler&& handler) :  // NOLINT
    connection_{handler.connection_},
    initialized_{handler.initialized_},
    enabled_{handler.enabled_},
    sender_bwe_{handler.sender_bwe_},
    sr_delay_data_{std::move(handler.sr_delay_data_)} {}


void SenderBandwidthEstimationHandler::enable() {
  enabled_ = true;
}

void SenderBandwidthEstimationHandler::disable() {
  enabled_ = false;
}

void SenderBandwidthEstimationHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
  }
  if (!connection_) {
    return;
  }
  initialized_ = true;
}

void SenderBandwidthEstimationHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  // calculate rtt
  // Pass REMBs
  // pass RR info + delay + packets sent
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (chead->isFeedback()) {
    if (chead->getBlockCount() == 0 && (chead->getLength() + 1) * 4  == packet->length) {
      ELOG_DEBUG("Ignoring empty RR");
      ctx->fireRead(packet);
      return;
    }
    if (chead->getSourceSSRC() != connection_->getVideoSinkSSRC()) {
      ELOG_DEBUG("Ignoring no video Packet sourceSSRC %u ssrc %u",
          chead->getSourceSSRC(), chead->getSSRC());
      ctx->fireRead(packet);
      return;
    }
    // We try to add it just in case it is not there yet (otherwise its noop)
    char* movingBuf = packet->data;
    int rtcpLength = 0;
    int totalLength = 0;
    int currentBlock = 0;
    uint32_t calculateLastSr, calculatedlsr;
    int64_t nowms;
    uint32_t delay;
    uint64_t sentts, bitrate;
    int estimated_bitrate;
    uint8_t estimated_loss;
    int64_t estimated_rtt;

    do {
      movingBuf+=rtcpLength;
      chead = reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength = (ntohs(chead->length) + 1) * 4;
      totalLength += rtcpLength;
      ELOG_DEBUG("%s ssrc %u, sourceSSRC %u, PacketType %u", connection_->toLog(),
          chead->getSSRC(),
          chead->getSourceSSRC(),
          chead->getPacketType());
      switch (chead->packettype) {
        case RTCP_Receiver_PT:
          ELOG_DEBUG("Analyzing Video RR: PacketLost %u, Ratio %u, currentBlock %d, blocks %d"
              ", sourceSSRC %u, ssrc %u changed to %u",
              chead->getLostPackets(),
              chead->getFractionLost(),
              currentBlock,
              chead->getBlockCount(),
              chead->getSourceSSRC(),
              chead->getSSRC(),
              connection_->getVideoSinkSSRC());
          // calculate RTT + Update receiver block
          calculateLastSr = chead->getLastSr();
          calculatedlsr = (chead->getDelaySinceLastSr() * 1000) / 65536;
          nowms = ClockUtils::timePointToMs(clock::now());
          for (auto it = sr_delay_data_.begin();
              it != sr_delay_data_.end(); ++it) {
            if ((*it)->sr_ntp == calculateLastSr) {
              sentts = (*it)->sr_send_time;
              delay = nowms - sentts - calculatedlsr;
              sender_bwe_->UpdateReceiverBlock(chead->getFractionLost(),
                  delay, packets_sent_, nowms);
              packets_sent_ = 0;
              ELOG_DEBUG("Calculated delay %u", delay);
              sender_bwe_->CurrentEstimate(&estimated_bitrate, &estimated_loss,
                  &estimated_rtt);
              ELOG_DEBUG("Estimated bitrate %u, loss %u, rtt %lld", estimated_bitrate,
                  estimated_loss, estimated_rtt);
            }
          }
          break;
        case RTCP_PS_Feedback_PT:
          if (chead->getBlockCount() == RTCP_AFB) {
            char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
            if (!strncmp(uniqueId, "REMB", 4)) {
              nowms = ClockUtils::timePointToMs(clock::now());
              bitrate = chead->getBrMantis() << chead->getBrExp();
              ELOG_DEBUG("REMB %llu", bitrate);
              sender_bwe_->UpdateReceiverEstimate(nowms, bitrate);
              // update bwe
            } else {
              ELOG_WARN("Unsupported AFB Packet not REMB")
            }
          }
          break;
        default:
          break;
      }
      currentBlock++;
    } while (totalLength < packet->length);
  }
  ctx->fireRead(packet);
}

void SenderBandwidthEstimationHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp()) {
    packets_sent_++;
  } else if (chead->getPacketType() == RTCP_Sender_PT) {
    analyzeSr(chead);
  }
  // account rtp packets sent
  // Analyze SR for delay
  ctx->fireWrite(packet);
}

void SenderBandwidthEstimationHandler::analyzeSr(RtcpHeader* chead) {
  // We try to add it just in case it is not there yet (otherwise its noop)
  ELOG_DEBUG("%s analyzeSR", connection_->toLog());
  if (chead->getSSRC() != connection_->getVideoSinkSSRC()) {
    ELOG_DEBUG("%s ignoring SR, ssrc %u", connection_->toLog(), chead->getSSRC());
  }
  uint64_t now = ClockUtils::timePointToMs(clock::now());
  uint32_t ntp;
  uint64_t total_ntp = chead->getNtpTimestamp();
  ntp = (total_ntp & (0xFFFFFFFF0000)) >> 16;
  sr_delay_data_.push_back(std::shared_ptr<SrDelayData>( new SrDelayData(ntp, now)));
  // We only store the last 20 sr
  if (sr_delay_data_.size() >= kMaxSrListSize) {
    sr_delay_data_.pop_front();
  }
}

}  // namespace erizo
