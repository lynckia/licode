#include "rtp/RtpPaddingGeneratorHandler.h"

#include <algorithm>
#include <string>

#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"
#include "./RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpPaddingGeneratorHandler, "rtp.RtpPaddingGeneratorHandler");

constexpr duration kStatsPeriod = std::chrono::milliseconds(100);
constexpr duration kFastStartMaxDuration = std::chrono::seconds(30);
constexpr uint8_t kMaxPaddingSize = 255;
constexpr uint64_t kMinMarkerRate = 3;
constexpr uint8_t kMaxFractionLostAllowed = .05 * 255;  // 5% of packet losts
constexpr uint64_t kInitialRembValue = 300000;

RtpPaddingGeneratorHandler::RtpPaddingGeneratorHandler(std::shared_ptr<erizo::Clock> the_clock) :
  clock_{the_clock}, connection_{nullptr}, max_video_bw_{0}, higher_sequence_number_{0},
  video_sink_ssrc_{0}, audio_source_ssrc_{0},
  number_of_full_padding_packets_{0}, last_padding_packet_size_{0},
  last_rate_calculation_time_{clock_->now()}, started_at_{clock_->now()},
  enabled_{false}, first_packet_received_{false},
  marker_rate_{std::chrono::milliseconds(100), 20, 1., clock_},
  padding_bitrate_{std::chrono::milliseconds(100), 10, 8., clock_},
  rtp_header_length_{12}, remb_value_{kInitialRembValue}, fast_start_{true} {}


void RtpPaddingGeneratorHandler::enable() {
}

void RtpPaddingGeneratorHandler::disable() {
}

void RtpPaddingGeneratorHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
    video_sink_ssrc_ = connection_->getVideoSinkSSRC();
    audio_source_ssrc_ = connection_->getAudioSinkSSRC();
    stats_ = pipeline->getService<Stats>();
  }

  auto quality_manager = pipeline->getService<QualityManager>();

  if (quality_manager->isPaddingEnabled() && !enabled_) {
    enablePadding();
  } else if (!quality_manager->isPaddingEnabled() && enabled_) {
    disablePadding();
  }

  auto processor = pipeline->getService<RtcpProcessor>();
  if (processor) {
    max_video_bw_ = processor->getMaxVideoBW();
  }
}

void RtpPaddingGeneratorHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtpUtils::forEachRRBlock(packet, [this](RtcpHeader *chead) {
    if (chead->packettype == RTCP_PS_Feedback_PT && chead->getBlockCount() == RTCP_AFB) {
      char *uniqueId = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
      if (!strncmp(uniqueId, "REMB", 4)) {
        remb_value_ = chead->getREMBBitRate();
      }
    } else if (fast_start_ && chead->packettype == RTCP_Receiver_PT && chead->getSourceSSRC() == audio_source_ssrc_) {
      if (chead->getFractionLost() > kMaxFractionLostAllowed) {
        ELOG_DEBUG("Fast start disabled");
        fast_start_ = false;
      }
    }
  });

  ctx->fireRead(packet);
}

void RtpPaddingGeneratorHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  bool is_higher_sequence_number = false;
  if (packet->type == VIDEO_PACKET && !chead->isRtcp()) {
    is_higher_sequence_number = isHigherSequenceNumber(packet);
    if (!first_packet_received_) {
      started_at_ = clock_->now();
    }
    first_packet_received_ = true;
    if (fast_start_ && clock_->now() - started_at_ > kFastStartMaxDuration) {
      ELOG_DEBUG("Fast start disabled");
      fast_start_ = false;
    }
  }

  ctx->fireWrite(packet);

  if (is_higher_sequence_number) {
    onVideoPacket(packet);
  }
}

void RtpPaddingGeneratorHandler::sendPaddingPacket(std::shared_ptr<dataPacket> packet, uint8_t padding_size) {
  if (padding_size == 0) {
    return;
  }

  SequenceNumber sequence_number = translator_.generate();

  auto padding_packet = RtpUtils::makePaddingPacket(packet, padding_size);

  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(padding_packet->data);

  rtp_header->setSeqNumber(sequence_number.output);
  padding_bitrate_ += padding_packet->length;

  getContext()->fireWrite(padding_packet);
}

void RtpPaddingGeneratorHandler::onPacketWithMarkerSet(std::shared_ptr<dataPacket> packet) {
  marker_rate_++;

  for (uint i = 0; i < number_of_full_padding_packets_; i++) {
    sendPaddingPacket(packet, kMaxPaddingSize);
  }
  sendPaddingPacket(packet, last_padding_packet_size_);
}

bool RtpPaddingGeneratorHandler::isHigherSequenceNumber(std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  rtp_header_length_ = rtp_header->getHeaderLength();
  uint16_t new_sequence_number = rtp_header->getSeqNumber();
  SequenceNumber sequence_number = translator_.get(new_sequence_number, false);
  rtp_header->setSeqNumber(sequence_number.output);
  if (first_packet_received_ && RtpUtils::sequenceNumberLessThan(new_sequence_number, higher_sequence_number_)) {
    return false;
  }
  higher_sequence_number_ = new_sequence_number;
  return true;
}

void RtpPaddingGeneratorHandler::onVideoPacket(std::shared_ptr<dataPacket> packet) {
  if (!enabled_) {
    return;
  }

  recalculatePaddingRate();

  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  if (rtp_header->getMarker()) {
    onPacketWithMarkerSet(packet);
  }
}

uint64_t RtpPaddingGeneratorHandler::getStat(std::string stat_name) {
  if (stats_->getNode()["total"].hasChild(stat_name)) {
    StatNode & stat = stats_->getNode()["total"][stat_name];
    return static_cast<MovingIntervalRateStat&>(stat).value();
  }
  return 0;
}

bool RtpPaddingGeneratorHandler::isTimeToCalculateBitrate() {
  return (clock_->now() - last_rate_calculation_time_) >= kStatsPeriod;
}

void RtpPaddingGeneratorHandler::recalculatePaddingRate() {
  if (!isTimeToCalculateBitrate()) {
    return;
  }

  last_rate_calculation_time_ = clock_->now();

  int64_t total_bitrate = getStat("bitrateCalculated");
  int64_t padding_bitrate = padding_bitrate_.value();
  int64_t media_bitrate = std::max(total_bitrate - padding_bitrate, int64_t(0));

  uint64_t target_bitrate = getTargetBitrate();

  int64_t target_padding_bitrate = target_bitrate - media_bitrate;

  if (target_padding_bitrate <= 0) {
    number_of_full_padding_packets_ = 0;
    last_padding_packet_size_ = 0;
    return;
  }

  uint64_t marker_rate = marker_rate_.value(std::chrono::seconds(2));
  marker_rate = std::max(marker_rate, kMinMarkerRate);
  uint64_t bytes_per_marker = target_padding_bitrate / (marker_rate * 8);
  number_of_full_padding_packets_ = bytes_per_marker / (kMaxPaddingSize + rtp_header_length_);
  last_padding_packet_size_ = bytes_per_marker % (kMaxPaddingSize + rtp_header_length_) - rtp_header_length_;
}

uint64_t RtpPaddingGeneratorHandler::getTargetBitrate() {
  uint64_t target_bitrate = remb_value_;

  if (!fast_start_) {
    target_bitrate = getStat("senderBitrateEstimation");
  }

  if (max_video_bw_ > 0) {
    target_bitrate = std::min(target_bitrate, max_video_bw_);
  }
  return target_bitrate;
}

void RtpPaddingGeneratorHandler::enablePadding() {
  enabled_ = true;
  number_of_full_padding_packets_ = 0;
  last_padding_packet_size_ = 0;
  last_rate_calculation_time_ = clock_->now();
}

void RtpPaddingGeneratorHandler::disablePadding() {
  enabled_ = false;
}

}  // namespace erizo
