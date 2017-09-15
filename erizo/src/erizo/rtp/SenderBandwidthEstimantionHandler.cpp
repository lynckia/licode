#include "./MediaDefinitions.h"
#include "rtp/SenderBandwidthEstimationHandler.h"

namespace erizo {

DEFINE_LOGGER(SenderBandwidthEstimationHandler, "rtp.SenderBandwidthEstimationHandler");

constexpr duration SenderBandwidthEstimationHandler::kMinUpdateEstimateInterval;

SenderBandwidthEstimationHandler::SenderBandwidthEstimationHandler(std::shared_ptr<Clock> the_clock) :
  connection_{nullptr}, bwe_listener_{nullptr}, clock_{the_clock}, initialized_{false}, enabled_{true},
  received_remb_{false}, period_packets_sent_{0}, estimated_bitrate_{0}, estimated_loss_{0},
  estimated_rtt_{0}, last_estimate_update_{clock::now()}, sender_bwe_{new SendSideBandwidthEstimation()} {
    sender_bwe_->SetSendBitrate(kStartSendBitrate);
  };

SenderBandwidthEstimationHandler::SenderBandwidthEstimationHandler(const SenderBandwidthEstimationHandler&& handler) :  // NOLINT
    connection_{handler.connection_},
    bwe_listener_{handler.bwe_listener_},
    clock_{handler.clock_},
    initialized_{handler.initialized_},
    enabled_{handler.enabled_},
    received_remb_{false},
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
    processor_ = pipeline->getService<RtcpProcessor>();
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

void SenderBandwidthEstimationHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (chead->isFeedback()) {
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
            if (chead->getSourceSSRC() != connection_->getVideoSinkSSRC()) {
              continue;
            }
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
            int64_t now_ms = ClockUtils::timePointToMs(clock_->now());
            uint32_t last_sr = chead->getLastSr();

            auto value = std::find_if(sr_delay_data_.begin(), sr_delay_data_.end(),
                [last_sr](const std::shared_ptr<SrDelayData> sr_info) {
                return sr_info->sr_ntp == last_sr;
                });
            // TODO(pedro) Implement alternative when there are no REMBs
            if (received_remb_ && value != sr_delay_data_.end()) {
                uint32_t delay = now_ms - (*value)->sr_send_time - delay_since_last_ms;
                ELOG_DEBUG("%s message: Updating Estimate with RR, fraction_lost: %u, "
                    "delay: %u, period_packets_sent_: %u",
                    connection_->toLog(), chead->getFractionLost(), delay, period_packets_sent_);
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
                received_remb_ = true;
                int64_t now_ms = ClockUtils::timePointToMs(clock_->now());
                uint64_t bitrate = chead->getBrMantis() << chead->getBrExp();
                uint64_t cappedBitrate = bitrate < processor_->getMaxVideoBW() ? bitrate : processor_->getMaxVideoBW();
                chead->setREMBBitRate(cappedBitrate);

                ELOG_DEBUG("%s message: Updating Estimate with REMB, bitrate %lu", connection_->toLog(),
                    cappedBitrate);
                sender_bwe_->UpdateReceiverEstimate(now_ms, cappedBitrate);
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
  ctx->fireRead(std::move(packet));
}

void SenderBandwidthEstimationHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp() && packet->type == VIDEO_PACKET) {
    period_packets_sent_++;
    time_point now = clock_->now();
    if (received_remb_ && now - last_estimate_update_ > kMinUpdateEstimateInterval) {
      sender_bwe_->UpdateEstimate(ClockUtils::timePointToMs(now));
      updateEstimate();
      last_estimate_update_ = now;
    }
  } else if (chead->getPacketType() == RTCP_Sender_PT &&
      chead->getSSRC() == connection_->getVideoSinkSSRC()) {
    analyzeSr(chead);
  }
  ctx->fireWrite(std::move(packet));
}

void SenderBandwidthEstimationHandler::analyzeSr(RtcpHeader* chead) {
  uint64_t now = ClockUtils::timePointToMs(clock_->now());
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
