#include "./MediaDefinitions.h"
#include "rtp/RtpSlideShowHandler.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpSlideShowHandler, "rtp.RtpSlideShowHandler");

RtpSlideShowHandler::RtpSlideShowHandler()
  : connection_{nullptr}, highest_seq_num_initialized_{false},
    highest_seq_num_ {0},
    slideshow_is_active_{false},
    current_keyframe_timestamp_{0} {}


void RtpSlideShowHandler::enable() {
}

void RtpSlideShowHandler::disable() {
}

void RtpSlideShowHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
  }
  setSlideShowMode(connection_->isSlideShowModeEnabled());
}

void RtpSlideShowHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (connection_->getVideoSinkSSRC() != chead->getSourceSSRC()) {
    ctx->fireRead(packet);
    return;
  }
  RtpUtils::forEachRRBlock(packet, [this](RtcpHeader *chead) {
    switch (chead->packettype) {
      case RTCP_Receiver_PT:
        {
          uint16_t incoming_seq_num = chead->getHighestSeqnum();
          ELOG_DEBUG("Received RR, highest %u", incoming_seq_num);
          SequenceNumber input_seq_num = translator_.reverse(incoming_seq_num);
          if (input_seq_num.type != SequenceNumberType::Valid) {
            break;
          }
          if (RtpUtils::sequenceNumberLessThan(input_seq_num.input, incoming_seq_num)) {
            chead->setSeqnumCycles(chead->getSeqnumCycles() - 1);
          }

          chead->setHighestSeqnum(input_seq_num.input);
          ELOG_DEBUG("Setting highest %u, output %u", input_seq_num.input, input_seq_num.output);
          break;
        }
      case RTCP_RTP_Feedback_PT:
        {
          ELOG_DEBUG("Received NACK, PID %u", chead->getNackPid());
          SequenceNumber input_seq_num = translator_.reverse(chead->getNackPid());
          if (input_seq_num.type == SequenceNumberType::Valid) {
            chead->setNackPid(input_seq_num.input);
            ELOG_DEBUG("Setting PID %u, output %u", input_seq_num.input, input_seq_num.output);
          }
          break;
        }
      default:
        break;
    }
  });
  ctx->fireRead(packet);
}

void RtpSlideShowHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);
  if (packet->type != VIDEO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireWrite(packet);
    return;
  }
  bool should_skip_packet = false;

  uint16_t packet_seq_num = rtp_header->getSeqNumber();
  if (slideshow_is_active_) {
    bool is_keyframe = false;
    RtpMap *codec = connection_->getRemoteSdpInfo().getCodecByExternalPayloadType(rtp_header->getPayloadType());
    if (codec && codec->encoding_name == "VP8") {
      is_keyframe = isVP8Keyframe(packet);
    } else if (codec && codec->encoding_name == "VP9") {
      is_keyframe = isVP9Keyframe(packet);
    }
    should_skip_packet = !is_keyframe;
  }
  maybeUpdateHighestSeqNum(rtp_header->getSeqNumber());
  SequenceNumber sequence_number_info = translator_.get(packet_seq_num, should_skip_packet);
  if (!should_skip_packet && sequence_number_info.type == SequenceNumberType::Valid) {
    rtp_header->setSeqNumber(sequence_number_info.output);
    ELOG_DEBUG("Writing packet incoming seq %u, output %u", packet_seq_num, sequence_number_info.output);
    ctx->fireWrite(packet);
  } else {
    ELOG_DEBUG("Skipped packet %u", packet_seq_num);
  }
}

bool RtpSlideShowHandler::isVP8Keyframe(std::shared_ptr<dataPacket> packet) {
  bool is_keyframe = false;
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  uint16_t seq_num = rtp_header->getSeqNumber();
  uint32_t timestamp = rtp_header->getTimestamp();

  if (packet->is_keyframe &&
      (RtpUtils::sequenceNumberLessThan(highest_seq_num_, seq_num) || !highest_seq_num_initialized_)) {
    is_keyframe = true;
    current_keyframe_timestamp_ = timestamp;
  } else if (timestamp == current_keyframe_timestamp_) {
    is_keyframe = true;
  }

  if (!highest_seq_num_initialized_) {
    highest_seq_num_ = rtp_header->getSeqNumber();
    highest_seq_num_initialized_ = true;
  }
  ELOG_DEBUG("packet is_keyframe %d, packet timestamp %u, current_keyframe timestamp %u,  result %d",
      packet->is_keyframe, rtp_header->getTimestamp(), current_keyframe_timestamp_, is_keyframe);
  return is_keyframe;
}

bool RtpSlideShowHandler::isVP9Keyframe(std::shared_ptr<dataPacket> packet) {
  return packet->is_keyframe;
}

void RtpSlideShowHandler::setSlideShowMode(bool active) {
  if (slideshow_is_active_ == active) {
    return;
  }

  if (active) {
    slideshow_is_active_ = true;
    connection_->setFeedbackReports(false, 0);
  } else {
    slideshow_is_active_ = false;
    connection_->setFeedbackReports(true, 0);
  }
}

void RtpSlideShowHandler::maybeUpdateHighestSeqNum(uint16_t seq_num) {
  if (RtpUtils::sequenceNumberLessThan(highest_seq_num_, seq_num)) {
    highest_seq_num_ = seq_num;
  }
}

}  // namespace erizo
