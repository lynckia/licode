#include "rtp/PLIPacerHandler.h"

#include "rtp/RtpUtils.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

namespace erizo {

DEFINE_LOGGER(PLIPacerHandler, "rtp.PLIPacerHandler");

constexpr duration PLIPacerHandler::kMinPLIPeriod;
constexpr duration PLIPacerHandler::kKeyframeTimeout;

PLIPacerHandler::PLIPacerHandler(std::shared_ptr<erizo::Clock> the_clock)
    : enabled_{true}, connection_{nullptr}, clock_{the_clock}, time_last_keyframe_{clock_->now()},
      waiting_for_keyframe_{false}, scheduled_pli_{-1},
      video_sink_ssrc_{0}, video_source_ssrc_{0} {}

void PLIPacerHandler::enable() {
  enabled_ = true;
}

void PLIPacerHandler::disable() {
  enabled_ = false;
}

void PLIPacerHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
    video_sink_ssrc_ = connection_->getVideoSinkSSRC();
    video_source_ssrc_ = connection_->getVideoSourceSSRC();
  }
}

void PLIPacerHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  if (enabled_ && packet->is_keyframe) {
    time_last_keyframe_ = clock_->now();
    waiting_for_keyframe_ = false;
    connection_->getWorker()->unschedule(scheduled_pli_);
    scheduled_pli_ = -1;
  }
  ctx->fireRead(packet);
}

void PLIPacerHandler::sendPLI() {
  getContext()->fireWrite(RtpUtils::createPLI(video_source_ssrc_, video_sink_ssrc_));
  scheduleNextPLI();
}

void PLIPacerHandler::sendFIR() {
  getContext()->fireWrite(RtpUtils::createFIR(video_source_ssrc_, video_sink_ssrc_));
  getContext()->fireWrite(RtpUtils::createFIR(video_source_ssrc_, video_sink_ssrc_));
  getContext()->fireWrite(RtpUtils::createFIR(video_source_ssrc_, video_sink_ssrc_));
  waiting_for_keyframe_ = false;
  scheduled_pli_ = -1;
}

void PLIPacerHandler::scheduleNextPLI() {
  if (!waiting_for_keyframe_ || !enabled_) {
    return;
  }
  std::weak_ptr<PLIPacerHandler> weak_this = shared_from_this();
  scheduled_pli_ = connection_->getWorker()->scheduleFromNow([weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      if (this_ptr->clock_->now() - this_ptr->time_last_keyframe_ >= kKeyframeTimeout) {
        this_ptr->sendFIR();
        return;
      }
      this_ptr->sendPLI();
    }
  }, kMinPLIPeriod);
}

void PLIPacerHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  if (enabled_ && RtpUtils::isPLI(packet)) {
    if (waiting_for_keyframe_) {
      return;
    }
    waiting_for_keyframe_ = true;
    scheduleNextPLI();
  }
  ctx->fireWrite(packet);
}

}  // namespace erizo
