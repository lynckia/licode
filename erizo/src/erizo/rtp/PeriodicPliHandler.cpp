#include "rtp/PeriodicPliHandler.h"

#include "rtp/RtpUtils.h"
#include "./MediaDefinitions.h"
#include "./MediaStream.h"

namespace erizo {

DEFINE_LOGGER(PeriodicPliHandler, "rtp.PeriodicPliHandler");

PeriodicPliHandler::PeriodicPliHandler(std::shared_ptr<erizo::Clock> the_clock)
    : enabled_{true}, stream_{nullptr}, clock_{the_clock},
      video_sink_ssrc_{0}, video_source_ssrc_{0}, keyframes_received_in_interval_{0},
      requested_periodic_plis_{false}, has_scheduled_pli_{false} {}

void PeriodicPliHandler::enable() {
  enabled_ = true;
}

void PeriodicPliHandler::disable() {
  enabled_ = false;
}

void PeriodicPliHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
    video_sink_ssrc_ = stream_->getVideoSinkSSRC();
    video_source_ssrc_ = stream_->getVideoSourceSSRC();
  }

  updateInterval(stream_->isRequestingPeriodicKeyframes(), stream_->getPeriodicKeyframesRequesInterval());
}

void PeriodicPliHandler::updateInterval(bool active, uint32_t interval_ms) {
  requested_periodic_plis_ = active;
  requested_interval_ = std::chrono::milliseconds(interval_ms);
  if (enabled_ && requested_periodic_plis_ && !has_scheduled_pli_) {
    ELOG_DEBUG("%s, message: Updating interval, requested_periodic_plis_: %u, interval: %u", stream_->toLog(),
        requested_periodic_plis_, ClockUtils::durationToMs(requested_interval_));
    scheduleNextPli(requested_interval_);
    has_scheduled_pli_ = true;
  }
}

void PeriodicPliHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  if (enabled_ && packet->is_keyframe) {
    std::for_each(packet->compatible_spatial_layers.begin(),
        packet->compatible_spatial_layers.end(),
        [this](const int spatial_layer) {
          if (spatial_layer == 0) {
            keyframes_received_in_interval_++;
          }
        });
    ELOG_DEBUG("%s, message: Received Keyframe, total from lowest layer in interval %u",
        stream_->toLog(), keyframes_received_in_interval_);
  }
  ctx->fireRead(std::move(packet));
}

void PeriodicPliHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireWrite(std::move(packet));
}

void PeriodicPliHandler::sendPLI() {
  getContext()->fireWrite(RtpUtils::createPLI(video_source_ssrc_, video_sink_ssrc_, HIGH_PRIORITY));
}


void PeriodicPliHandler::scheduleNextPli(duration next_pli_time) {
  if (!enabled_) {
    return;
  }
  ELOG_INFO("%s, message: scheduleNextKeyframe, keyframes_received_in_interval_: %u, next_keyframe %u",
      stream_->toLog(), keyframes_received_in_interval_, ClockUtils::durationToMs(next_pli_time));;
  std::weak_ptr<PeriodicPliHandler> weak_this = shared_from_this();
  stream_->getWorker()->scheduleFromNow([weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      if (this_ptr->requested_periodic_plis_) {
        ELOG_DEBUG("%s, message: Maybe Sending PLI, keyframes_received_in_interval_: %u",
            this_ptr->stream_->toLog(), this_ptr->keyframes_received_in_interval_);
        if (this_ptr->keyframes_received_in_interval_ <= 1) {
          ELOG_DEBUG("%s, message: Will send PLI, keyframes_received_in_interval_: %u",
              this_ptr->stream_->toLog(), this_ptr->keyframes_received_in_interval_);
          this_ptr->sendPLI();
        }
        this_ptr->keyframes_received_in_interval_ = 0;
        ELOG_DEBUG("%s, message: Scheduling next pli in : %u",
            this_ptr->stream_->toLog(), ClockUtils::durationToMs(this_ptr->requested_interval_));
        this_ptr->scheduleNextPli(this_ptr->requested_interval_);
      } else {
        ELOG_DEBUG("%s, message: Not scheduling more PLIs: %u",
            this_ptr->stream_->toLog(), this_ptr->keyframes_received_in_interval_);
        this_ptr->has_scheduled_pli_ = false;
        this_ptr->sendPLI();
      }
    }
  }, next_pli_time);
}
}  // namespace erizo

