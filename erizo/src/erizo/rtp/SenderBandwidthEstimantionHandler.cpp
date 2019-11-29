#include "rtp/SenderBandwidthEstimationHandler.h"

#include <utility>

#include "./MediaDefinitions.h"
#include "rtp/RtpUtils.h"
#include "./MediaStream.h"

namespace erizo {

DEFINE_LOGGER(SenderBandwidthEstimationHandler, "rtp.SenderBandwidthEstimationHandler");

constexpr duration SenderBandwidthEstimationHandler::kMinUpdateEstimateInterval;

SenderBandwidthEstimationHandler::SenderBandwidthEstimationHandler(std::shared_ptr<Clock> the_clock) :
  connection_{nullptr}, bwe_listener_{nullptr}, clock_{the_clock}, initialized_{false}, enabled_{true},
  received_remb_{false}, estimated_bitrate_{0}, estimated_loss_{0},
  estimated_rtt_{0}, last_estimate_update_{clock::now()}, sender_bwe_{new SendSideBandwidthEstimation()},
  max_rr_delay_data_size_{0}, max_sr_delay_data_size_{0} {
    sender_bwe_->SetBitrates(kStartSendBitrate, kMinSendBitrate, kMaxSendBitrate);
  }

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
    sr_delay_data_{std::move(handler.sr_delay_data_)},
    rr_delay_data_{std::move(handler.rr_delay_data_)},
    max_rr_delay_data_size_{handler.max_sr_delay_data_size_},
    max_sr_delay_data_size_{handler.max_rr_delay_data_size_} {}


void SenderBandwidthEstimationHandler::enable() {
  enabled_ = true;
}

void SenderBandwidthEstimationHandler::disable() {
  enabled_ = false;
}

void SenderBandwidthEstimationHandler::notifyUpdate() {
  if (initialized_) {
    updateMaxListSizes();
    return;
  }
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
  }
  if (!connection_) {
    return;
  }
  updateMaxListSizes();
  stats_ = pipeline->getService<Stats>();
  if (!stats_) {
    return;
  }
  initialized_ = true;
}

void SenderBandwidthEstimationHandler::updateMaxListSizes() {
  size_t num_streams = 0;
  connection_->forEachMediaStream([&num_streams] (std::shared_ptr<MediaStream> media_stream) {
    if (!media_stream->isPublisher()) {
      num_streams++;
    }
  });
  max_sr_delay_data_size_ = num_streams * kMaxSrListSize;
  max_rr_delay_data_size_ = num_streams;
  updateReceiverBlockFromList();
}

void SenderBandwidthEstimationHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtpUtils::forEachRtcpBlock(packet, [this](RtcpHeader *chead) {
    switch (chead->packettype) {
      case RTCP_Receiver_PT:
        {
          // calculate RTT + Update receiver block
          uint32_t delay_since_last_ms = (chead->getDelaySinceLastSr() * 1000) / 65536;
          int64_t now_ms = ClockUtils::timePointToMs(clock_->now());
          uint32_t last_sr = chead->getLastSr();
          uint32_t ssrc = chead->getSourceSSRC();

          auto value = std::find_if(sr_delay_data_.begin(), sr_delay_data_.end(),
              [ssrc, last_sr](const std::shared_ptr<SrDelayData> sr_info) {
                  return sr_info->ssrc == ssrc && sr_info->sr_ntp == last_sr;
              });
          ELOG_DEBUG("%s, Analyzing Video RR: PacketLost %u, Ratio %u, blocks %d"
              ", sourceSSRC %u, ssrc %u, last_sr %u, remb_received %d, found %d, max_size: %d, size: %d",
              connection_->toLog(),
              chead->getLostPackets(),
              chead->getFractionLost(),
              chead->getBlockCount(),
              chead->getSourceSSRC(),
              chead->getSSRC(),
              chead->getLastSr(),
              received_remb_,
              value != sr_delay_data_.end(),
              max_rr_delay_data_size_,
              rr_delay_data_.size());
          if (received_remb_ && value != sr_delay_data_.end()) {
              uint32_t delay = now_ms - (*value)->sr_send_time - delay_since_last_ms;
              rr_delay_data_.push_back(
                std::make_shared<RrDelayData>(chead->getSourceSSRC(), delay, chead->getFractionLost()));
              updateReceiverBlockFromList();
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
              uint64_t remb_bitrate =  chead->getBrMantis() << chead->getBrExp();
              uint64_t bitrate = estimated_bitrate_ != 0 ? estimated_bitrate_ : remb_bitrate;

              // We update the REMB with the latest estimation
              chead->setREMBBitRate(bitrate);
              ELOG_DEBUG("%s message: Updating estimate REMB, bitrate: %lu, estimated_bitrate %lu, remb_bitrate %lu",
                  connection_->toLog(), bitrate, estimated_bitrate_, remb_bitrate);
              sender_bwe_->UpdateReceiverEstimate(now_ms, remb_bitrate);
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
  });
  ctx->fireRead(std::move(packet));
}

void SenderBandwidthEstimationHandler::updateReceiverBlockFromList() {
  if (rr_delay_data_.size() < max_rr_delay_data_size_) {
    return;
  }
  // TODO(pedro) Implement alternative when there are no REMBs
  if (received_remb_) {
    uint32_t total_packets_lost = 0;
    uint32_t total_packets_sent = 0;
    uint64_t avg_delay = 0;
    uint32_t rr_delay_data_size = rr_delay_data_.size();
    int64_t now_ms = ClockUtils::timePointToMs(clock_->now());
    std::for_each(rr_delay_data_.begin(), rr_delay_data_.end(),
        [&avg_delay, &total_packets_lost, rr_delay_data_size, &total_packets_sent, this]
        (const std::shared_ptr<RrDelayData> &rr_info) {
          auto packets_sent_ssrc = period_packets_sent_.find(rr_info->ssrc);
          if (packets_sent_ssrc != period_packets_sent_.end()) {
            total_packets_lost += rr_info->fraction_lost * packets_sent_ssrc->second / 255;
            total_packets_sent += packets_sent_ssrc->second;
            avg_delay += rr_info->delay / rr_delay_data_size;
          }
    });
    if (total_packets_sent > 0) {
      uint32_t fraction_lost = total_packets_lost * 255 / total_packets_sent;
      ELOG_DEBUG("%s message: Updating Estimate with RR, fraction_lost: %u, "
                "delay: %u, period_packets_sent_: %u",
                connection_->toLog(), fraction_lost, avg_delay, total_packets_sent);
      sender_bwe_->UpdateReceiverBlock(fraction_lost, avg_delay, total_packets_sent, now_ms);
      updateEstimate();
    }
  }
  period_packets_sent_.clear();
  rr_delay_data_.clear();
}

void SenderBandwidthEstimationHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!chead->isRtcp()) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    uint32_t ssrc = rtp_header->getSSRC();
    auto packets_sent_for_ssrc = period_packets_sent_.find(ssrc);
    if (packets_sent_for_ssrc != period_packets_sent_.end()) {
      packets_sent_for_ssrc->second++;
    } else {
      period_packets_sent_.emplace(ssrc, 1);
    }
    if (packet->type == VIDEO_PACKET) {
      time_point now = clock_->now();
      if (received_remb_ && now - last_estimate_update_ > kMinUpdateEstimateInterval) {
        sender_bwe_->UpdateEstimate(ClockUtils::timePointToMs(now));
        updateEstimate();
        last_estimate_update_ = now;
      }
    }
  } else if (chead->getPacketType() == RTCP_Sender_PT) {
    analyzeSr(chead);
  }
  ctx->fireWrite(std::move(packet));
}

void SenderBandwidthEstimationHandler::analyzeSr(RtcpHeader* chead) {
  uint64_t now = ClockUtils::timePointToMs(clock_->now());
  uint32_t ntp;
  uint32_t ssrc = chead->getSSRC();
  ntp = chead->get32MiddleNtp();
  ELOG_DEBUG("%s message: adding incoming SR to list, ntp: %u", connection_->toLog(), ntp);
  sr_delay_data_.push_back(std::make_shared<SrDelayData>(ssrc, ntp, now));
  if (sr_delay_data_.size() >= max_sr_delay_data_size_) {
    sr_delay_data_.pop_front();
  }
}

void SenderBandwidthEstimationHandler::updateEstimate() {
  sender_bwe_->CurrentEstimate(&estimated_bitrate_, &estimated_loss_,
      &estimated_rtt_);
  if (stats_) {
    stats_->getNode()["total"].insertStat("senderBitrateEstimation",
      CumulativeStat{static_cast<uint64_t>(estimated_bitrate_)});
  }
  ELOG_DEBUG("%s message: estimated bitrate %d, loss %u, rtt %ld",
      connection_->toLog(), estimated_bitrate_, estimated_loss_, estimated_rtt_);
  if (bwe_listener_) {
    bwe_listener_->onBandwidthEstimate(estimated_bitrate_, estimated_loss_, estimated_rtt_);
  }
}
}  // namespace erizo
