#include "./MediaDefinitions.h"
#include "rtp/SenderBandwidthEstimationHandler.h"

namespace erizo {

DEFINE_LOGGER(SenderBandwidthEstimationHandler, "rtp.SenderBandwidthEstimationHandler");

SenderBandwidthEstimationHandler::SenderBandwidthEstimationHandler() :
  connection_{nullptr}, bwe_listener_{nullptr}, initialized_{false}, enabled_{true},
  period_packets_sent_{0}, estimated_bitrate_{0}, estimated_loss_{0}, estimated_rtt_{0},
  sender_bwe_{new SendSideBandwidthEstimation()} {};

SenderBandwidthEstimationHandler::SenderBandwidthEstimationHandler(const SenderBandwidthEstimationHandler&& handler) :  // NOLINT
    connection_{handler.connection_},
    bwe_listener_{handler.bwe_listener_},
    initialized_{handler.initialized_},
    enabled_{handler.enabled_},
    period_packets_sent_{handler.period_packets_sent_},
    estimated_bitrate_{handler.estimated_bitrate_},
    estimated_loss_{handler.estimated_loss_},
    estimated_rtt_{handler.estimated_rtt_},
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
  stats_ = pipeline->getService<Stats>();
  if (!stats_) {
    return;
  }
  initialized_ = true;
}

void SenderBandwidthEstimationHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (chead->isFeedback() && chead->getSourceSSRC() == connection_->getVideoSinkSSRC()) {
    char* packet_pointer = packet->data;
    int rtcp_length = 0;
    int total_length = 0;
    int current_block = 0;

    do {
      packet_pointer+=rtcp_length;
      chead = reinterpret_cast<RtcpHeader*>(packet_pointer);
      rtcp_length = (ntohs(chead->length) + 1) * 4;
      total_length += rtcp_length;
      ELOG_DEBUG("%s ssrc %u, sourceSSRC %u, PacketType %u", connection_->toLog(),
          chead->getSSRC(),
          chead->getSourceSSRC(),
          chead->getPacketType());
      switch (chead->packettype) {
        case RTCP_Receiver_PT:
          {
            ELOG_DEBUG("%s, Analyzing Video RR: PacketLost %u, Ratio %u, current_block %d, blocks %d"
                ", sourceSSRC %u, ssrc %u",
                connection_->toLog(),
                chead->getLostPackets(),
                chead->getFractionLost(),
                current_block,
                chead->getBlockCount(),
                chead->getSourceSSRC(),
                chead->getSSRC());
            // calculate RTT + Update receiver block
            uint32_t delay_since_last_ms = (chead->getDelaySinceLastSr() * 1000) / 65536;
            int64_t now_ms = ClockUtils::timePointToMs(clock::now());
            uint32_t last_sr = chead->getLastSr();

            auto value = std::find_if(sr_delay_data_.begin(), sr_delay_data_.end(),
                [last_sr](const std::shared_ptr<SrDelayData> sr_info) {
                return sr_info->sr_ntp == last_sr;
                });
            if (value != sr_delay_data_.end()) {
                uint32_t delay = now_ms - (*value)->sr_send_time - delay_since_last_ms;
                sender_bwe_->UpdateReceiverBlock(chead->getFractionLost(),
                    delay, period_packets_sent_, now_ms);
                period_packets_sent_ = 0;
                updateEstimate();
            }
          }
          break;
        case RTCP_PS_Feedback_PT:
          {
            if (chead->getBlockCount() == RTCP_AFB) {
              char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
              if (!strncmp(uniqueId, "REMB", 4)) {
                int64_t now_ms = ClockUtils::timePointToMs(clock::now());
                uint64_t bitrate = chead->getBrMantis() << chead->getBrExp();
                ELOG_DEBUG("%s message: Updating Estimate with REMB, bitrate %lu", connection_->toLog(),
                    bitrate);
                sender_bwe_->UpdateReceiverEstimate(now_ms, bitrate);
                sender_bwe_->UpdateEstimate(now_ms);
                updateEstimate();
              } else {
                ELOG_DEBUG("%s message: Unsupported AFB Packet not REMB", connection_->toLog());
              }
            }
          }
          break;
        default:
          break;
      }
      current_block++;
    } while (total_length < packet->length);
  }
  ctx->fireRead(packet);
}

void SenderBandwidthEstimationHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && packet->type == VIDEO_PACKET) {
    period_packets_sent_++;
  } else if (chead->getPacketType() == RTCP_Sender_PT &&
      chead->getSSRC() == connection_->getVideoSinkSSRC()) {
    analyzeSr(chead);
  }
  ctx->fireWrite(packet);
}

void SenderBandwidthEstimationHandler::analyzeSr(RtcpHeader* chead) {
  uint64_t now = ClockUtils::timePointToMs(clock::now());
  uint32_t ntp;
  ntp = chead->get32MiddleNtp();
  ELOG_DEBUG("%s message: adding incoming SR to list, ntp: %u", connection_->toLog(), ntp);
  sr_delay_data_.push_back(std::shared_ptr<SrDelayData>( new SrDelayData(ntp, now)));
  if (sr_delay_data_.size() >= kMaxSrListSize) {
    sr_delay_data_.pop_front();
  }
}

void SenderBandwidthEstimationHandler::updateEstimate() {
  sender_bwe_->CurrentEstimate(&estimated_bitrate_, &estimated_loss_,
      &estimated_rtt_);
  stats_->getNode()["total"].insertStat("senderBitrateEstimation",
      CumulativeStat{static_cast<uint64_t>(estimated_bitrate_)});
  ELOG_DEBUG("%s message: estimated bitrate %d, loss %u, rtt %ld",
      connection_->toLog(), estimated_bitrate_, estimated_loss_, estimated_rtt_);
  if (bwe_listener_) {
    bwe_listener_->onBandwidthEstimate(estimated_bitrate_, estimated_loss_, estimated_rtt_);
  }
}
}  // namespace erizo
