#include "rtp/SenderBandwidthEstimationHandler.h"

#include <utility>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/test/explicit_key_value_config.h"
#include "webrtc/api/units/data_rate.h"
#include "webrtc/api/units/timestamp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

#include "./WebRtcConnection.h"
#include "./MediaDefinitions.h"
#include "rtp/RtpUtils.h"
#include "./MediaStream.h"
#include "./Stats.h"

namespace erizo {

DEFINE_LOGGER(SenderBandwidthEstimationHandler, "rtp.SenderBandwidthEstimationHandler");

const uint16_t SenderBandwidthEstimationHandler::kMaxSrListSize;
const uint32_t SenderBandwidthEstimationHandler::kStartSendBitrate;
const uint32_t SenderBandwidthEstimationHandler::kMinSendBitrate;
const uint32_t SenderBandwidthEstimationHandler::kMinSendBitrateLimit;
const uint32_t SenderBandwidthEstimationHandler::kMaxSendBitrate;
constexpr duration SenderBandwidthEstimationHandler::kMinUpdateEstimateInterval;

SenderBandwidthEstimationHandler::SenderBandwidthEstimationHandler(std::shared_ptr<Clock> the_clock) :
  connection_{nullptr}, clock_{the_clock}, initialized_{false}, enabled_{true},
  received_remb_{false}, estimated_bitrate_{0}, estimated_loss_{0},
  estimated_rtt_{0}, last_estimate_update_{clock::now()},
  max_rr_delay_data_size_{0}, max_sr_delay_data_size_{0},
  transport_wide_seqnum_{1} {
    webrtc::test::ExplicitKeyValueConfig key_value_config("WebRTC-Bwe-LossBasedControl/Enabled:true/");
      // ",BwRampupUpperBoundFactor:1.2."
      // ",CandidateFactors:0.9|1.1,HigherBwBiasFactor:0.01,"
      // "InherentLossLowerBound:0.001,InherentLossUpperBoundBwBalance:14kbps,"
      // "InherentLossUpperBoundOffset:0.9,InitialInherentLossEstimate:0.01,"
      // "NewtonIterations:2,NewtonStepSize:0.4,ObservationWindowSize:15,"
      // "SendingRateSmoothingFactor:0.01,"
      // "InstantUpperBoundTemporalWeightFactor:0.97,"
      // "InstantUpperBoundBwBalance:90kbps,"
      // "InstantUpperBoundLossOffset:0.1,TemporalWeightFactor:0.98"
      // ",ObservationDurationLowerBound:200ms/"
    sender_bwe_.reset(new SendSideBandwidthEstimation(&key_value_config));
    sender_bwe_->SetBitrates(
      webrtc::DataRate::BitsPerSec(kStartSendBitrate),
      webrtc::DataRate::BitsPerSec(kMinSendBitrate),
      webrtc::DataRate::BitsPerSec(kMaxSendBitrate),
      getNowTimestamp());

    feedback_adapter_.reset(new TransportFeedbackAdapter());
    acknowledged_bitrate_estimator_ = AcknowledgedBitrateEstimatorInterface::Create(&key_value_config);
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
    updateNumberOfStreams();
    return;
  }
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
  }
  if (!connection_) {
    return;
  }
  updateNumberOfStreams();
  stats_ = pipeline->getService<Stats>();
  if (!stats_) {
    return;
  }
  initialized_ = true;
}

void SenderBandwidthEstimationHandler::updateNumberOfStreams() {
  size_t num_streams = 0;
  connection_->forEachMediaStream([&num_streams] (std::shared_ptr<MediaStream> media_stream) {
    if (!media_stream->isPublisher()) {
      num_streams++;
    }
  });
  // update max list sizes
  max_sr_delay_data_size_ = num_streams * kMaxSrListSize;
  max_rr_delay_data_size_ = num_streams;
  updateReceiverBlockFromList();
  // if there are streams update bitrate limits
  if (num_streams > 0) {
    uint32_t min_send_bitrate = std::min(kMinSendBitrate * static_cast<uint32_t>(num_streams), kMinSendBitrateLimit);
    ELOG_DEBUG("SetBitrates, estimated: %u, min: %u, max: %u", estimated_bitrate_, min_send_bitrate, kMaxSendBitrate);

    sender_bwe_->SetBitrates(
      absl::optional<webrtc::DataRate>(),
      webrtc::DataRate::BitsPerSec(min_send_bitrate),
      webrtc::DataRate::BitsPerSec(kMaxSendBitrate),
      getNowTimestamp());
  }
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
              ELOG_DEBUG("Received REMB");
              received_remb_ = true;
              uint64_t remb_bitrate =  chead->getBrMantis() << chead->getBrExp();
              uint64_t bitrate = estimated_bitrate_ != 0 ? estimated_bitrate_ : remb_bitrate;

              // We update the REMB with the latest estimation
              chead->setREMBBitRate(bitrate);
              ELOG_DEBUG("%s message: Updating estimate REMB, bitrate: %lu, estimated_bitrate %lu, remb_bitrate %lu",
                  connection_->toLog(), bitrate, estimated_bitrate_, remb_bitrate);
              sender_bwe_->UpdateReceiverEstimate(getNowTimestamp(), webrtc::DataRate::BitsPerSec(remb_bitrate));
              updateEstimate();
            } else {
              ELOG_DEBUG("%s message: Unsupported AFB Packet not REMB", connection_->toLog());
            }
          }
        }
        break;
      case RTCP_RTP_Feedback_PT:
        if (chead->isTransportWideFeedback()) {
          received_remb_ = true;
          std::unique_ptr<TransportFeedback> fb = TransportFeedback::ParseFrom(
            reinterpret_cast<uint8_t*>(chead),
            (ntohs(chead->length) + 1) * 4);
          if (fb) {
            absl::optional<webrtc::TransportPacketsFeedback> report =
              feedback_adapter_->ProcessTransportFeedback(*fb, getNowTimestamp());
            if (report.has_value()) {
              acknowledged_bitrate_estimator_->IncomingPacketFeedbackVector(
                report.value().SortedByReceiveTime());
              auto acknowledged_bitrate = acknowledged_bitrate_estimator_->bitrate();
              if (!acknowledged_bitrate) {
                acknowledged_bitrate = acknowledged_bitrate_estimator_->PeekRate();
              }
              sender_bwe_->SetAcknowledgedRate(acknowledged_bitrate,
                report.value().feedback_time);
              sender_bwe_->IncomingPacketFeedbackVector(report.value());
              sender_bwe_->UpdateEstimate(getNowTimestamp());
              updateEstimate();
              last_estimate_update_ = clock_->now();
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

void SenderBandwidthEstimationHandler::onTransportFeedbackReport(const webrtc::TransportPacketsFeedback& report) {
}

void SenderBandwidthEstimationHandler::updateReceiverBlockFromList() {
  if (rr_delay_data_.size() < max_rr_delay_data_size_) {
    return;
  }
  if (received_remb_) {
    uint32_t total_packets_lost = 0;
    uint32_t total_packets_sent = 0;
    uint64_t avg_delay = 0;
    uint32_t rr_delay_data_size = rr_delay_data_.size();
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
      ELOG_DEBUG("%s message: Updating Estimate with RR, packets_lost: %u, "
                "delay: %u, period_packets_sent_: %u",
                connection_->toLog(), total_packets_lost, avg_delay, total_packets_sent);
      sender_bwe_->UpdateRtt(webrtc::TimeDelta::Millis(avg_delay), getNowTimestamp());
      sender_bwe_->UpdatePacketsLost(total_packets_lost, total_packets_sent, getNowTimestamp());
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
      packet->transport_sequence_number = transport_wide_seqnum_++;
      webrtc::RtpPacketSendInfo packet_info;
      packet_info.media_ssrc = ssrc;
      packet_info.transport_sequence_number = packet->transport_sequence_number.value();
      packet_info.packet_type = webrtc::RtpPacketMediaType::kVideo;
      packet_info.length = packet->length;
      feedback_adapter_->AddPacket(webrtc::RtpPacketSendInfo(packet_info), 0, getNowTimestamp());
      rtc::SentPacket sent_packet(packet->transport_sequence_number.value(),
      ClockUtils::timePointToMs(clock_->now()));
      sent_packet.info.packet_size_bytes = packet->length;
      absl::optional<webrtc::SentPacket> processed_packet = feedback_adapter_->ProcessSentPacket(sent_packet);
      if (processed_packet.has_value()) {
        sender_bwe_->OnSentPacket(processed_packet.value());
      }
      if (received_remb_ && now - last_estimate_update_ > kMinUpdateEstimateInterval) {
        sender_bwe_->UpdateEstimate(getNowTimestamp());
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
  int64_t new_estimate = sender_bwe_->GetEstimatedLinkCapacity().bps();
  int64_t new_target = sender_bwe_->target_rate().bps();
  if (new_estimate == estimated_bitrate_ && new_target == estimated_target_) {
    return;
  }
  estimated_bitrate_ = new_estimate;
  estimated_target_ = new_target;
  if (stats_) {
    stats_->getNode()["total"].insertStat("senderBitrateEstimation",
      CumulativeStat{static_cast<uint64_t>(estimated_bitrate_)});
    stats_->getNode()["total"].insertStat("senderBitrateEstimationTarget",
      CumulativeStat{static_cast<uint64_t>(estimated_target_)});
  }
  ELOG_DEBUG("%s message: estimated bitrate %lu, estimated_target %lu loss %u, rtt %ld",
      connection_->toLog(), estimated_bitrate_, estimated_target_, estimated_loss_, estimated_rtt_);
  if (auto listener = bwe_listener_.lock()) {
    listener->onBandwidthEstimate(estimated_bitrate_, estimated_loss_, estimated_rtt_);
  }
}

webrtc::Timestamp SenderBandwidthEstimationHandler::getNowTimestamp() {
    int64_t now_ms = ClockUtils::timePointToMs(clock_->now());
    return webrtc::Timestamp::Millis(now_ms);
}
}  // namespace erizo
