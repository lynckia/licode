#include "rtp/RtpPaddingManagerHandler.h"

#include <algorithm>
#include <string>
#include <inttypes.h>

#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"
#include "./MediaStream.h"
#include "./RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpPaddingManagerHandler, "rtp.RtpPaddingManagerHandler");

static constexpr duration kStatsPeriod = std::chrono::milliseconds(100);
constexpr duration RtpPaddingManagerHandler::kMinDurationToSendPaddingAfterBweDecrease;
constexpr duration RtpPaddingManagerHandler::kMaxDurationInRecoveryFromBwe;
static constexpr double kBitrateComparisonMargin = 1.3;
static constexpr uint64_t kInitialBitrate = 300000;
static constexpr uint64_t kUnnasignedBitrateMargin = 50000;

RtpPaddingManagerHandler::RtpPaddingManagerHandler(std::shared_ptr<erizo::Clock> the_clock) :
  initialized_{false},
  clock_{the_clock},
  last_rate_calculation_time_{clock_->now()},
  connection_{nullptr},
  last_estimated_bandwidth_{0} {
}

void RtpPaddingManagerHandler::enable() {
}

void RtpPaddingManagerHandler::disable() {
}

void RtpPaddingManagerHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    stats_ = pipeline->getService<Stats>();
    if (!stats_) {
      return;
    }
    connection_ = pipeline->getService<WebRtcConnection>().get();
    if (!connection_) {
      return;
    }
    stats_->getNode()["total"].insertStat("paddingBitrate",
        MovingIntervalRateStat{std::chrono::milliseconds(100), 30, 8., clock_});
    stats_->getNode()["total"].insertStat("videoBitrate",
        MovingIntervalRateStat{std::chrono::milliseconds(100), 30, 8., clock_});
  }

  if (!connection_) {
    return;
  }

  initialized_ = true;
}

bool RtpPaddingManagerHandler::isTimeToCalculateBitrate() {
  return initialized_ && (clock_->now() - last_rate_calculation_time_) >= kStatsPeriod;
}

void RtpPaddingManagerHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireRead(std::move(packet));
}

void RtpPaddingManagerHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (packet->is_padding) {
    stats_->getNode()["total"]["paddingBitrate"] += packet->length;
  } else if (packet->type == VIDEO_PACKET && !chead->isRtcp()) {
    stats_->getNode()["total"]["videoBitrate"] += packet->length;
  }

  recalculatePaddingRate();

  ctx->fireWrite(packet);
}

void RtpPaddingManagerHandler::recalculatePaddingRate() {
  if (!isTimeToCalculateBitrate()) {
    return;
  }

  StatNode &total = stats_->getNode()["total"];

  if (!total.hasChild("senderBitrateEstimation") ||
      !total.hasChild("videoBitrate")) {
    return;
  }

  last_rate_calculation_time_ = clock_->now();

  int64_t media_bitrate = total["videoBitrate"].value();
  int64_t estimated_bandwidth = total["senderBitrateEstimation"].value();

  int64_t target_bitrate = getTotalTargetBitrate();

  if (target_bitrate == 0) {
    target_bitrate = kInitialBitrate;
  }

  int64_t target_padding_bitrate = std::max(target_bitrate - media_bitrate, int64_t(0));
  int64_t available_bw = std::max(estimated_bandwidth - media_bitrate, int64_t(0));

  target_padding_bitrate = std::min(target_padding_bitrate, available_bw);

  bool estimated_is_high_enough = estimated_bandwidth > (target_bitrate * kBitrateComparisonMargin);
  bool has_unnasigned_bitrate = false;
  bool has_connection_target_bitrate = connection_->getConnectionTargetBw() > 0;
  if (stats_->getNode()["total"].hasChild("unnasignedBitrate")) {
    has_unnasigned_bitrate =
        stats_->getNode()["total"]["unnasignedBitrate"].value() > kUnnasignedBitrateMargin &&
        !has_connection_target_bitrate;
  }
  if (estimated_is_high_enough || has_unnasigned_bitrate) {
    target_padding_bitrate = 0;
  }

  bool remb_is_decreasing = estimated_bandwidth < last_estimated_bandwidth_;
  last_estimated_bandwidth_ = estimated_bandwidth;
  double step = 1.0;
  duration time_since_bwe_decreased_ = clock_->now() - last_time_bwe_decreased_;
  if (remb_is_decreasing) {
    ELOG_DEBUG("remb is decreasing");
    target_padding_bitrate = 0;
    last_time_bwe_decreased_ = clock_->now();
  } else if (time_since_bwe_decreased_ < kMinDurationToSendPaddingAfterBweDecrease) {
    ELOG_DEBUG("backoff period");
    target_padding_bitrate = 0;
  } else if (time_since_bwe_decreased_ > kMinDurationToSendPaddingAfterBweDecrease) {
    step = static_cast<double>(time_since_bwe_decreased_.count()) / kMaxDurationInRecoveryFromBwe.count();
    ELOG_DEBUG("ramping up period time since %d, max %d, calculated step %f",
    std::chrono::duration_cast<std::chrono::milliseconds>(time_since_bwe_decreased_).count(),
    std::chrono::duration_cast<std::chrono::milliseconds>(kMaxDurationInRecoveryFromBwe).count(),
    step);
    step = std::min(step, 1.0);
    target_padding_bitrate = target_padding_bitrate * step;
  }

  ELOG_DEBUG("%s Calculated: target %d, bwe %d, last_bwe %d, media %d, target %d, bwe enough %d"
  " step %f, remb_is_decreasing %d",
    connection_->toLog(),
    target_padding_bitrate,
    estimated_bandwidth,
    last_estimated_bandwidth_,
    media_bitrate,
    target_bitrate,
    estimated_is_high_enough,
    step,
    remb_is_decreasing);
  distributeTotalTargetPaddingBitrate(target_padding_bitrate);
}

void RtpPaddingManagerHandler::distributeTotalTargetPaddingBitrate(int64_t bitrate) {
  size_t num_streams = 0;
  connection_->forEachMediaStream([&num_streams]
    (std::shared_ptr<MediaStream> media_stream) {
      if (!media_stream->isPublisher()) {
        num_streams++;
      }
    });
  stats_->getNode()["total"].insertStat("numberOfStreams",
      CumulativeStat{static_cast<uint64_t>(num_streams)});
  if (num_streams == 0) {
    return;
  }
  int64_t bitrate_per_stream = bitrate / num_streams;
  connection_->forEachMediaStreamAsync([bitrate_per_stream]
    (std::shared_ptr<MediaStream> media_stream) {
      if (media_stream->isPublisher()) {
        return;
      }
      media_stream->setTargetPaddingBitrate(bitrate_per_stream);
  });
}

int64_t RtpPaddingManagerHandler::getTotalTargetBitrate() {
  int64_t target_bitrate = 0;
  connection_->forEachMediaStream([&target_bitrate]
    (std::shared_ptr<MediaStream> media_stream) {
      if (media_stream->isPublisher()) {
        return;
      }
      target_bitrate += media_stream->getTargetVideoBitrate();
    });
  target_bitrate = std::max(target_bitrate, static_cast<int64_t>(connection_->getConnectionTargetBw()));
  stats_->getNode()["total"].insertStat("targetBitrate",
    CumulativeStat{static_cast<uint64_t>(target_bitrate)});

  return target_bitrate;
}
}  // namespace erizo
