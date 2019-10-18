#include "rtp/PliPriorityHandler.h"

#include "rtp/RtpUtils.h"
#include "./MediaDefinitions.h"
#include "./MediaStream.h"

namespace erizo {

DEFINE_LOGGER(PliPriorityHandler, "rtp.PliPriorityHandler");

constexpr duration PliPriorityHandler::kLowPriorityPliPeriod;

PliPriorityHandler::PliPriorityHandler(std::shared_ptr<erizo::Clock> the_clock)
    : enabled_{true}, stream_{nullptr}, clock_{the_clock},
      video_sink_ssrc_{0}, video_source_ssrc_{0}, plis_received_in_interval_{0}, first_received_{false} {}

void PliPriorityHandler::enable() {
  enabled_ = true;
}

void PliPriorityHandler::disable() {
  enabled_ = false;
}

void PliPriorityHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
    video_sink_ssrc_ = stream_->getVideoSinkSSRC();
    video_source_ssrc_ = stream_->getVideoSourceSSRC();
  }
}

void PliPriorityHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  if (enabled_ && packet->is_keyframe) {
    ELOG_DEBUG("%s, message: Received Keyframe, resetting plis", stream_->toLog());
    plis_received_in_interval_ = 0;  // reset the pli counter because we have keyframe
  }
  ctx->fireRead(std::move(packet));
}

void PliPriorityHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  if (enabled_ && RtpUtils::isPLI(packet) && packet->priority == LOW_PRIORITY) {
    if (!first_received_) {
      ELOG_DEBUG("%s, message: First PLI received - sending it, %d", stream_->toLog(), packet->priority);
      first_received_ = true;
      sendPLI();
      schedulePeriodicPlis();
      return;
    }
    plis_received_in_interval_++;
    ELOG_DEBUG("%s, message: Accounting received PLI, priority %d, total %u",
        stream_->toLog(), packet->priority, plis_received_in_interval_);
    return;
  }
  ctx->fireWrite(std::move(packet));
}

void PliPriorityHandler::sendPLI() {
  getContext()->fireWrite(RtpUtils::createPLI(video_source_ssrc_, video_sink_ssrc_, LOW_PRIORITY));
}

void PliPriorityHandler::schedulePeriodicPlis() {
  if (!enabled_) {
    return;
  }
  ELOG_INFO("%s, message: Starting low priority plis scheduler, plis_received_in_interval_: %u",
      stream_->toLog(), plis_received_in_interval_);
  std::weak_ptr<PliPriorityHandler> weak_this = shared_from_this();
  stream_->getWorker()->scheduleEvery([weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      if (this_ptr->plis_received_in_interval_ > 0) {
        ELOG_DEBUG("%s, message: Sending Low priority PLI, plis_received_in_interval_: %u",
            this_ptr->stream_->toLog(), this_ptr->plis_received_in_interval_);
        this_ptr->plis_received_in_interval_ = 0;
        this_ptr->sendPLI();
        return true;
      } else {
      //  we could stop the periodic task here and restart it when a PLI is requested
        ELOG_DEBUG("%s, message: No Low priority Plis to send, plis_received_in_interval_: %u",
            this_ptr->stream_->toLog(), this_ptr->plis_received_in_interval_);
        return true;
      }
    }
    return false;
  }, kLowPriorityPliPeriod);
}
}  // namespace erizo

