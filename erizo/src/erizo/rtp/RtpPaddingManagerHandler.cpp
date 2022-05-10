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
static constexpr double kBitrateComparisonMargin = 1.1;
static constexpr uint64_t kInitialBitrate = 300000;
static constexpr uint64_t kUnnasignedBitrateMargin = 200000;

RtpPaddingManagerHandler::RtpPaddingManagerHandler(std::shared_ptr<erizo::Clock> the_clock) :
  initialized_{false},
  clock_{the_clock},
  last_rate_calculation_time_{clock_->now()},
  last_mode_change_{clock_->now()},
  connection_{nullptr},
  last_estimated_bandwidth_{0},
  can_recover_{true},
  current_mode_{PaddingManagerMode::START} {
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
    stats_->getNode()["total"].insertStat("paddingMode",
        CumulativeStat{current_mode_});
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

PaddingManagerMode RtpPaddingManagerHandler::getCurrentPaddingMode() {
  ELOG_DEBUG("Getting current padding mode - %u", current_mode_);
  return current_mode_;
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
  int64_t estimated_target = total["senderBitrateEstimationTarget"].value();

  // TODO(pedro): IN REMB mode estimated_target ~= estimated -> That has to be taken into account

  int64_t target_bitrate = getTotalTargetBitrate();

  if (target_bitrate == 0) {
    target_bitrate = kInitialBitrate;
  }
  int64_t target_padding_bitrate = std::max(target_bitrate - media_bitrate, int64_t(0));
  bool remb_sharp_drop = estimated_bandwidth < last_estimated_bandwidth_*kBweSharpDropThreshold;
  if (current_mode_ == PaddingManagerMode::RECOVER) {  // if in recover mode any drop will stop it
    remb_sharp_drop = estimated_bandwidth < last_estimated_bandwidth_;
  }
  int64_t available_bitrate = std::max(estimated_target - media_bitrate, int64_t(0));

  ELOG_DEBUG("Is sharp drop? last_estimated*k %f, new_estimated %u", last_estimated_bandwidth_*kBweSharpDropThreshold,
    estimated_bandwidth);
  //  Maybe Trigger Hold Mode
  if (remb_sharp_drop) {
    estimated_before_drop_ = last_estimated_bandwidth_;
    ELOG_DEBUG("%s Detected sharp BWE drop, estimated_before_drop_: %lu, estimated_bandwidth: %u",
        connection_->toLog(), estimated_before_drop_, estimated_bandwidth);
    if (current_mode_ == PaddingManagerMode::RECOVER) {
      ELOG_DEBUG("%s, Sharp drop in recover mode", connection_->toLog());
      can_recover_ = false;
    }
    forceModeSwitch(PaddingManagerMode::HOLD);
  }
  last_estimated_bandwidth_ = estimated_bandwidth;

  //  Check if it's time to change mode
  maybeTriggerTimedModeChanges();

  switch (current_mode_) {
    case PaddingManagerMode::START:
      {
        // available_bitrate = std::max(estimated_bandwidth*kStartModeFactor - media_bitrate, static_cast<double>(0));
        target_padding_bitrate = std::min(target_padding_bitrate, available_bitrate);  // never send more than max
        break;
      }
    case PaddingManagerMode::STABLE:
      {
        can_recover_ = true;
        target_padding_bitrate = std::min(target_padding_bitrate, available_bitrate);
        bool has_unnasigned_bitrate = false;
        bool has_connection_target_bitrate = connection_->getConnectionTargetBw() > 0;
        bool estimated_is_high_enough = estimated_bandwidth > (target_bitrate * kBitrateComparisonMargin);
        if (stats_->getNode()["total"].hasChild("unnasignedBitrate")) {
          has_unnasigned_bitrate =
            stats_->getNode()["total"]["unnasignedBitrate"].value() > kUnnasignedBitrateMargin &&
            !has_connection_target_bitrate;
        }
        if (estimated_is_high_enough || has_unnasigned_bitrate) {
          target_padding_bitrate = 0;
        }
        break;
      }
    case PaddingManagerMode::HOLD:
      {
        target_padding_bitrate = 0;
        break;
      }
    case PaddingManagerMode::RECOVER:
      {
        available_bitrate = std::max(estimated_before_drop_*kRecoverBweFactor - media_bitrate, static_cast<double>(0));
        target_padding_bitrate = std::min(available_bitrate, target_bitrate);
        break;
      }
  }

  ELOG_DEBUG("Padding stats: target_bitrate %lu, target_padding_bitrate %lu, current_mode_ %u "
  "estimated_bitrate %lu, media_bitrate: %lu, available_bw: %lu",
    target_bitrate,
    target_padding_bitrate,
    current_mode_,
    estimated_bandwidth,
    media_bitrate,
    available_bitrate,
    current_mode_);

  distributeTotalTargetPaddingBitrate(target_padding_bitrate);
}

void RtpPaddingManagerHandler::forceModeSwitch(PaddingManagerMode new_mode) {
  ELOG_DEBUG("%s switching mode, current_mode_ %u, new_mode %u, ms since last change %lu",
      connection_->toLog(),
      current_mode_,
      new_mode,
      std::chrono::duration_cast<std::chrono::milliseconds>(clock_->now() - last_mode_change_));
  current_mode_ = new_mode;
  last_mode_change_ = clock_->now();
  stats_->getNode()["total"].insertStat("paddingMode",
      CumulativeStat{current_mode_});
}

void RtpPaddingManagerHandler::maybeTriggerTimedModeChanges() {
  duration time_in_current_mode = clock_->now() - last_mode_change_;
  switch (current_mode_) {
    case PaddingManagerMode::START:
      {
        if (time_in_current_mode > kMaxDurationInStartMode) {
          ELOG_DEBUG("%s Start mode is over, switching to stable", connection_->toLog());
          forceModeSwitch(PaddingManagerMode::STABLE);
        }
        break;
      }
    case PaddingManagerMode::STABLE:
      break;
    case PaddingManagerMode::HOLD:
      {
        if (time_in_current_mode > kMaxDurationInHoldMode) {
          if (can_recover_) {
            ELOG_DEBUG("%s Hold mode is over, switching to recover", connection_->toLog());
            forceModeSwitch(PaddingManagerMode::RECOVER);
          } else {
            ELOG_DEBUG("%s Hold mode is over, switching to stable", connection_->toLog());
            forceModeSwitch(PaddingManagerMode::STABLE);
          }
        }
        break;
      }
    case PaddingManagerMode::RECOVER:
      {
        if (time_in_current_mode > kMaxDurationInRecoverMode) {
          ELOG_DEBUG("%s Recover is successful, switching to stable", connection_->toLog());
          forceModeSwitch(PaddingManagerMode::STABLE);
        }
        break;
      }
  }
}

void RtpPaddingManagerHandler::distributeTotalTargetPaddingBitrate(int64_t bitrate) {
  size_t num_streams = 0;
  connection_->forEachMediaStream([&num_streams]
    (std::shared_ptr<MediaStream> media_stream) {
      if (media_stream->canSendPadding()) {
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
      if (!media_stream->canSendPadding()) {
        return;
      }
      ELOG_DEBUG("Setting Target %u", bitrate_per_stream);
      media_stream->setTargetPaddingBitrate(bitrate_per_stream);
  });
}

int64_t RtpPaddingManagerHandler::getTotalTargetBitrate() {
  int64_t target_bitrate = 0;
  connection_->forEachMediaStream([&target_bitrate]
    (std::shared_ptr<MediaStream> media_stream) {
      if (!media_stream->canSendPadding()) {
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
