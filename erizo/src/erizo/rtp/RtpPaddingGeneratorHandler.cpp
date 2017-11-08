#include "rtp/RtpPaddingGeneratorHandler.h"

#include <algorithm>
#include <string>

#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"
#include "./RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpPaddingGeneratorHandler, "rtp.RtpPaddingGeneratorHandler");

constexpr duration kStatsPeriod = std::chrono::milliseconds(100);
constexpr uint8_t kMaxPaddingSize = 255;
constexpr uint64_t kMinMarkerRate = 3;
constexpr uint64_t kInitialBitrate = 300000;
constexpr uint64_t kPaddingBurstSize = 255 * 10;

RtpPaddingGeneratorHandler::RtpPaddingGeneratorHandler(std::shared_ptr<erizo::Clock> the_clock) :
  clock_{the_clock}, connection_{nullptr}, max_video_bw_{0}, higher_sequence_number_{0},
  video_sink_ssrc_{0}, audio_source_ssrc_{0},
  number_of_full_padding_packets_{0}, last_padding_packet_size_{0},
  last_rate_calculation_time_{clock_->now()}, started_at_{clock_->now()},
  enabled_{false}, first_packet_received_{false},
  marker_rate_{std::chrono::milliseconds(100), 20, 1., clock_},
  rtp_header_length_{12},
  bucket_{kInitialBitrate, kPaddingBurstSize, clock_},
  scheduled_task_{std::make_shared<ScheduledTaskReference>()} {}



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
    stats_->getNode()["total"].insertStat("paddingBitrate",
        MovingIntervalRateStat{std::chrono::milliseconds(100), 30, 8., clock_});
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

void RtpPaddingGeneratorHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireRead(std::move(packet));
}

void RtpPaddingGeneratorHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  bool is_higher_sequence_number = false;
  if (packet->type == VIDEO_PACKET && !chead->isRtcp()) {
    connection_->getWorker()->unschedule(scheduled_task_);
    is_higher_sequence_number = isHigherSequenceNumber(packet);
    if (!first_packet_received_) {
      started_at_ = clock_->now();
    }
    first_packet_received_ = true;
  }

  ctx->fireWrite(packet);

  if (is_higher_sequence_number) {
    onVideoPacket(std::move(packet));
  }
}

void RtpPaddingGeneratorHandler::sendPaddingPacket(std::shared_ptr<DataPacket> packet, uint8_t padding_size) {
  if (padding_size == 0) {
    return;
  }

  if (!bucket_.consume(padding_size)) {
    return;
  }

  SequenceNumber sequence_number = translator_.generate();

  auto padding_packet = RtpUtils::makePaddingPacket(packet, padding_size);

  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(padding_packet->data);

  rtp_header->setSeqNumber(sequence_number.output);
  stats_->getNode()["total"]["paddingBitrate"] += padding_packet->length;
  getContext()->fireWrite(std::move(padding_packet));
}

void RtpPaddingGeneratorHandler::onPacketWithMarkerSet(std::shared_ptr<DataPacket> packet) {
  marker_rate_++;

  for (uint i = 0; i < number_of_full_padding_packets_; i++) {
    sendPaddingPacket(packet, kMaxPaddingSize);
  }
  sendPaddingPacket(packet, last_padding_packet_size_);
  std::weak_ptr<RtpPaddingGeneratorHandler> weak_this = shared_from_this();
  scheduled_task_ = connection_->getWorker()->scheduleFromNow([packet, weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      this_ptr->onPacketWithMarkerSet(packet);
    }
  }, std::chrono::milliseconds(1000 / kMinMarkerRate));
}

bool RtpPaddingGeneratorHandler::isHigherSequenceNumber(std::shared_ptr<DataPacket> packet) {
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

void RtpPaddingGeneratorHandler::onVideoPacket(std::shared_ptr<DataPacket> packet) {
  if (!enabled_) {
    return;
  }

  recalculatePaddingRate();

  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  if (rtp_header->getMarker()) {
    onPacketWithMarkerSet(std::move(packet));
  }
}

uint64_t RtpPaddingGeneratorHandler::getStat(std::string stat_name) {
  if (stats_->getNode()[video_sink_ssrc_].hasChild(stat_name)) {
    StatNode & stat = stats_->getNode()[video_sink_ssrc_][stat_name];
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
  int64_t padding_bitrate = stats_->getNode()["total"]["paddingBitrate"].value();
  int64_t media_bitrate = std::max(total_bitrate - padding_bitrate, int64_t(0));

  uint64_t target_bitrate = getTargetBitrate();

  int64_t target_padding_bitrate = target_bitrate - media_bitrate;
  // TODO(pedro): figure out a burst size that makes sense here
  bucket_.reset(std::max(target_padding_bitrate, int64_t(0)), kPaddingBurstSize);

  if (target_padding_bitrate <= 0) {
    number_of_full_padding_packets_ = 0;
    last_padding_packet_size_ = 0;
    return;
  }

  uint64_t marker_rate = marker_rate_.value(std::chrono::milliseconds(500));
  marker_rate = std::max(marker_rate, kMinMarkerRate);
  uint64_t bytes_per_marker = target_padding_bitrate / (marker_rate * 8);
  number_of_full_padding_packets_ = bytes_per_marker / (kMaxPaddingSize + rtp_header_length_);
  last_padding_packet_size_ = bytes_per_marker % (kMaxPaddingSize + rtp_header_length_) - rtp_header_length_;
}

uint64_t RtpPaddingGeneratorHandler::getTargetBitrate() {
  uint64_t target_bitrate = kInitialBitrate;

  if (stats_->getNode()["total"].hasChild("senderBitrateEstimation")) {
    target_bitrate = static_cast<CumulativeStat&>(stats_->getNode()["total"]["senderBitrateEstimation"]).value();
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
