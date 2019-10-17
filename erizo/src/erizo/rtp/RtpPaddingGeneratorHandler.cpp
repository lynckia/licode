#include "rtp/RtpPaddingGeneratorHandler.h"

#include <algorithm>
#include <string>
#include <inttypes.h>

#include "./MediaDefinitions.h"
#include "./MediaStream.h"
#include "./RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpPaddingGeneratorHandler, "rtp.RtpPaddingGeneratorHandler");

constexpr uint8_t kMaxPaddingSize = 255;
constexpr uint64_t kMinMarkerRate = 3;
constexpr uint64_t kInitialBitrate = 300000;
constexpr uint8_t kMaxBurstPackets = 200;
constexpr uint8_t kSlideShowBurstPackets = 20;

RtpPaddingGeneratorHandler::RtpPaddingGeneratorHandler(std::shared_ptr<erizo::Clock> the_clock) :
  clock_{the_clock}, stream_{nullptr}, higher_sequence_number_{0},
  video_sink_ssrc_{0}, audio_source_ssrc_{0},
  number_of_full_padding_packets_{0}, last_padding_packet_size_{0},
  started_at_{clock_->now()},
  enabled_{false}, first_packet_received_{false},
  slideshow_mode_active_ {false},
  marker_rate_{std::chrono::milliseconds(100), 20, 1., clock_},
  rtp_header_length_{12},
  bucket_{kInitialBitrate, kSlideShowBurstPackets * kMaxPaddingSize, clock_},
  scheduled_task_{std::make_shared<ScheduledTaskReference>()} {
  }

void RtpPaddingGeneratorHandler::enable() {
}

void RtpPaddingGeneratorHandler::disable() {
}

void RtpPaddingGeneratorHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
    video_sink_ssrc_ = stream_->getVideoSinkSSRC();
    audio_source_ssrc_ = stream_->getAudioSinkSSRC();
    stats_ = pipeline->getService<Stats>();
    stats_->getNode()["total"].insertStat("paddingBitrate",
        MovingIntervalRateStat{std::chrono::milliseconds(100), 30, 8., clock_});
  }

  if (!stream_) {
    return;
  }

  uint64_t target_padding_bitrate = stream_->getTargetPaddingBitrate();

  recalculatePaddingRate(target_padding_bitrate);

  slideshow_mode_active_ = stream_->isSlideShowModeEnabled();
}

void RtpPaddingGeneratorHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireRead(std::move(packet));
}

void RtpPaddingGeneratorHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  bool is_higher_sequence_number = false;
  if (packet->type == VIDEO_PACKET && !chead->isRtcp()) {
    stream_->getWorker()->unschedule(scheduled_task_);
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
  scheduled_task_ = stream_->getWorker()->scheduleFromNow([packet, weak_this] {
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

  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  if (rtp_header->getMarker()) {
    onPacketWithMarkerSet(std::move(packet));
  }
}

void RtpPaddingGeneratorHandler::recalculatePaddingRate(uint64_t target_padding_bitrate) {
  // TODO(pedro): figure out a burst size that makes sense here
  bucket_.reset(target_padding_bitrate, getBurstSize());

  if (target_padding_bitrate == 0) {
    enabled_ = false;
    number_of_full_padding_packets_ = 0;
    last_padding_packet_size_ = 0;
    return;
  }

  enabled_ = true;

  uint64_t marker_rate = marker_rate_.value(std::chrono::milliseconds(500));
  marker_rate = std::max(marker_rate, kMinMarkerRate);
  // TODO(javier): There are arithmetic exceptions in the line following this if clause, so I'm
  // trying to figure out the cause. I'll remove this check if it happens in production and fix it.
  if (marker_rate > std::numeric_limits<uint64_t>::max() / 8) {
    ELOG_WARN("message: Marker Rate too high %" PRIu64, marker_rate);
  }
  uint64_t bytes_per_marker = target_padding_bitrate / (marker_rate * 8);
  number_of_full_padding_packets_ = bytes_per_marker / (kMaxPaddingSize + rtp_header_length_);
  last_padding_packet_size_ = bytes_per_marker % (kMaxPaddingSize + rtp_header_length_) - rtp_header_length_;
}

uint64_t RtpPaddingGeneratorHandler::getBurstSize() {
  uint64_t burstPackets = kSlideShowBurstPackets;
  if (!slideshow_mode_active_) {
    burstPackets = std::min((number_of_full_padding_packets_ + 1), (uint64_t)kMaxBurstPackets);
  }
  return burstPackets * kMaxPaddingSize;
}

}  // namespace erizo
