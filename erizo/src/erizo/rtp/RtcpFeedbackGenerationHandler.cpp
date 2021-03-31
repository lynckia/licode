#include "rtp/RtcpFeedbackGenerationHandler.h"
#include "./MediaStream.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtcpFeedbackGenerationHandler, "rtp.RtcpFeedbackGenerationHandler");

RtcpFeedbackGenerationHandler::RtcpFeedbackGenerationHandler(bool nacks_enabled,
    std::shared_ptr<Clock> the_clock)
  : stream_{nullptr}, enabled_{true}, initialized_{false}, nacks_enabled_{nacks_enabled}, clock_{the_clock} {}

void RtcpFeedbackGenerationHandler::enable() {
  enabled_ = true;
}

void RtcpFeedbackGenerationHandler::disable() {
  enabled_ = false;
}

void RtcpFeedbackGenerationHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  // Pass packets to RR and NACK Generator
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);

  if (!initialized_) {
    ctx->fireRead(std::move(packet));
    return;
  }

  if (chead->getPacketType() == RTCP_Sender_PT) {
    uint32_t ssrc = chead->getSSRC();
    auto generator_it = generators_map_.find(ssrc);
    if (generator_it != generators_map_.end()) {
      generator_it->second->rr_generator->handleSr(packet);
    } else {
      ELOG_DEBUG("message: no RrGenerator found, ssrc: %u", ssrc);
    }
    ctx->fireRead(std::move(packet));
    return;
  }
  bool should_send_rr = false;
  bool should_send_nack = false;

  if (!chead->isRtcp()) {
    RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
    uint32_t ssrc = head->getSSRC();
    bool is_audio = stream_->isAudioSourceSSRC(ssrc) || stream_->isAudioSinkSSRC(ssrc);
    auto generator_it = generators_map_.find(ssrc);
    std::shared_ptr<RtcpGeneratorPair> generator;
    if (generator_it == generators_map_.end()) {
      generator = std::make_shared<RtcpGeneratorPair>();
      generators_map_[ssrc] = generator;
      packetType type = is_audio ? AUDIO_PACKET : VIDEO_PACKET;
      generator->rr_generator = std::make_shared<RtcpRrGenerator>(ssrc, type, clock_);
      ELOG_DEBUG("%s, message: Initialized rrGenerator, ssrc: %u, audio: %d", stream_->toLog(), ssrc, is_audio);

      // TODO(pedro) detect if nacks are enabled here with the negotiated SDP scanning the rtp_mappings
      if (!is_audio && nacks_enabled_) {
        ELOG_DEBUG("%s, message: Initialized video nack generator, ssrc %u", stream_->toLog(), ssrc);
        generator->nack_generator = std::make_shared<RtcpNackGenerator>(ssrc, clock_);
      }
    } else {
      generator = generator_it->second;
    }

    should_send_rr = generator->rr_generator->handleRtpPacket(packet);
    if (generator->nack_generator) {
      should_send_nack = generator->nack_generator->handleRtpPacket(packet);
    }

    if (should_send_rr || should_send_nack) {
      std::shared_ptr<DataPacket> rtcp_packet = generator->rr_generator->generateReceiverReport();
      notifyReceiverReportInfo(rtcp_packet, is_audio);
      if (nacks_enabled_ && generator->nack_generator != nullptr) {
        generator->nack_generator->addNackPacketToRr(rtcp_packet);
      }
      ctx->fireWrite(std::move(rtcp_packet));
    }
  }
  ctx->fireRead(std::move(packet));
}

void RtcpFeedbackGenerationHandler::notifyReceiverReportInfo(const std::shared_ptr<DataPacket> &packet, bool is_audio) {
  MediaStream *media_stream = stream_;
  RtpUtils::forEachRtcpBlock(packet, [media_stream, is_audio](RtcpHeader *chead) {
    if (chead->isReceiverReport()) {
      uint8_t fraction_lost = chead->getFractionLost();
      std::string kind = is_audio ? "audio" : "video";
      media_stream->deliverEvent(std::make_shared<PublisherRtpInfoEvent>(kind, fraction_lost));
    }
  });
}

void RtcpFeedbackGenerationHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  ctx->fireWrite(std::move(packet));
}

void RtcpFeedbackGenerationHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  stream_ = pipeline->getService<MediaStream>().get();
  if (!stream_) {
    return;
  }

  initialized_ = true;
}

}  // namespace erizo
