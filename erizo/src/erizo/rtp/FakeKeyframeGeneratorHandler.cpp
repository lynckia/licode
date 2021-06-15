#include "rtp/FakeKeyframeGeneratorHandler.h"

#include <algorithm>
#include <string>

#include "./MediaDefinitions.h"
#include "./MediaStream.h"
#include "./RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(FakeKeyframeGeneratorHandler, "rtp.FakeKeyframeGeneratorHandler");

constexpr uint64_t kPliPeriodMs = 300;

FakeKeyframeGeneratorHandler::FakeKeyframeGeneratorHandler() :
  stream_{nullptr}, enabled_{true}, first_keyframe_received_{false}, plis_scheduled_{false},
  video_source_ssrc_{0}, video_sink_ssrc_{0} {
}

void FakeKeyframeGeneratorHandler::enable() {
  enabled_ = true;
}

void FakeKeyframeGeneratorHandler::disable() {
  enabled_ = false;
}

void FakeKeyframeGeneratorHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
  }
  if (stream_) {
    video_source_ssrc_ = stream_->getVideoSourceSSRC();
    video_sink_ssrc_ = stream_->getVideoSinkSSRC();
  }
}

void FakeKeyframeGeneratorHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireRead(std::move(packet));
}

void FakeKeyframeGeneratorHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!enabled_) {
    ctx->fireWrite(std::move(packet));
    return;
  }

  if (!first_keyframe_received_ && packet->type == VIDEO_PACKET && !chead->isRtcp()) {
    if (stream_->getCurrentState() == CONN_READY) {  // TODO(pedro): Find a solution for this in all handlers
      if (!packet->is_keyframe) {
        maybeSendAndSchedulePLIs();
        ELOG_DEBUG("Building a black keyframe from packet");
        auto keyframe_packet = transformIntoKeyframePacket(packet);
        ctx->fireWrite(keyframe_packet);
        return;
      } else {
        ELOG_DEBUG("First part of a keyframe received, stop rewriting packets");
        first_keyframe_received_ = true;
      }
    } else {
      ELOG_DEBUG("Transport is not ready - not transforming packet");
    }
  }
  ctx->fireWrite(std::move(packet));
}

std::shared_ptr<DataPacket> FakeKeyframeGeneratorHandler::transformIntoKeyframePacket
  (std::shared_ptr<DataPacket> packet) {
    if (packet->codec == "VP8") {
      auto keyframe_packet = RtpUtils::makeVP8BlackKeyframePacket(packet);
      packet->is_keyframe = true;
      return keyframe_packet;
    } else {
      ELOG_DEBUG("Generate keyframe packet is not available for codec %s", packet->codec);
      return packet;
    }
  }

void FakeKeyframeGeneratorHandler::maybeSendAndSchedulePLIs() {
  if (!plis_scheduled_) {
    plis_scheduled_ = true;
    ELOG_DEBUG("Scheduling PLIs");
    sendPLI();
    schedulePLI();
  }
}

void FakeKeyframeGeneratorHandler::sendPLI() {
  getContext()->fireRead(RtpUtils::createPLI(video_sink_ssrc_, video_source_ssrc_));
}

void FakeKeyframeGeneratorHandler::schedulePLI() {
  std::weak_ptr<FakeKeyframeGeneratorHandler> weak_this = shared_from_this();
  stream_->getWorker()->scheduleEvery([weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      if (!this_ptr->first_keyframe_received_) {
        ELOG_DEBUG("Sending PLI in FakeGenerator, scheduling another");
        this_ptr->sendPLI();
        return true;
      } else {
        ELOG_DEBUG("Stop sending scheduled PLI packets");
        return false;
      }
    }
    return false;
  }, std::chrono::milliseconds(kPliPeriodMs));
}
}  // namespace erizo
