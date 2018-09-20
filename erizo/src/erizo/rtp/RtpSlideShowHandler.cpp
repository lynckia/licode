#include "rtp/RtpSlideShowHandler.h"
#include "MediaStream.h"
#include <vector>

#include "./MediaDefinitions.h"
#include "rtp/RtpUtils.h"


namespace erizo {

DEFINE_LOGGER(RtpSlideShowHandler, "rtp.RtpSlideShowHandler");

RtpSlideShowHandler::RtpSlideShowHandler(std::shared_ptr<Clock> the_clock)
  : clock_{the_clock}, stream_{nullptr}, highest_seq_num_initialized_{false},
    is_building_keyframe_ {false},
    highest_seq_num_ {0},
    packets_received_while_building_{0},
    first_keyframe_seq_num_{0},
    slideshow_is_active_{false},
    current_keyframe_timestamp_{0},
    last_timestamp_received_{0},
    keyframe_buffer_{kMaxKeyframeSize},
    last_keyframe_sent_time_{clock_->now()} {}


void RtpSlideShowHandler::enable() {
}

void RtpSlideShowHandler::disable() {
}

void RtpSlideShowHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !stream_) {
    stream_ = pipeline->getService<MediaStream>().get();
  }
  bool fallback_slideshow_enabled = pipeline->getService<QualityManager>()->isFallbackFreezeEnabled();
  bool manual_slideshow_enabled = stream_->isSlideShowModeEnabled();
  if (fallback_slideshow_enabled) {
    ELOG_DEBUG("SlideShow fallback mode enabled");
  } else {
    ELOG_DEBUG("SlideShow fallback mode disabled");
  }
  setSlideShowMode(fallback_slideshow_enabled || manual_slideshow_enabled);
}

void RtpSlideShowHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (stream_->getVideoSinkSSRC() != chead->getSourceSSRC()) {
    ctx->fireRead(std::move(packet));
    return;
  }
  RtpUtils::forEachRtcpBlock(packet, [this](RtcpHeader *chead) {
    switch (chead->packettype) {
      case RTCP_Receiver_PT:
        {
          uint16_t incoming_seq_num = chead->getHighestSeqnum();
          SequenceNumber input_seq_num = translator_.reverse(incoming_seq_num);
          if (input_seq_num.type != SequenceNumberType::Valid) {
            break;
          }
          if (RtpUtils::sequenceNumberLessThan(input_seq_num.input, incoming_seq_num)) {
            chead->setSeqnumCycles(chead->getSeqnumCycles() - 1);
          }

          chead->setHighestSeqnum(input_seq_num.input);
          break;
        }
      case RTCP_RTP_Feedback_PT:
        {
          SequenceNumber input_seq_num = translator_.reverse(chead->getNackPid());
          if (input_seq_num.type == SequenceNumberType::Valid) {
            chead->setNackPid(input_seq_num.input);
          }
          break;
        }
      default:
        break;
    }
  });
  ctx->fireRead(std::move(packet));
}

void RtpSlideShowHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);
  if (packet->type != VIDEO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireWrite(std::move(packet));
    return;
  }
  bool should_skip_packet = false;

  last_timestamp_received_ = rtp_header->getTimestamp();

  uint16_t packet_seq_num = rtp_header->getSeqNumber();
  bool is_keyframe = false;
  RtpMap *codec = stream_->getRemoteSdpInfo()->getCodecByExternalPayloadType(rtp_header->getPayloadType());
  if (codec && (codec->encoding_name == "VP8" || codec->encoding_name == "H264")) {
    is_keyframe = isVP8OrH264Keyframe(packet);
  } else if (codec && codec->encoding_name == "VP9") {
    is_keyframe = isVP9Keyframe(packet);
  }
  if (slideshow_is_active_) {
    should_skip_packet = !is_keyframe;

    if (is_keyframe) {
      storeKeyframePacket(packet);
    }

    if (is_building_keyframe_) {
      consolidateKeyframe();
    }
    maybeSendStoredKeyframe();
  }
  maybeUpdateHighestSeqNum(rtp_header->getSeqNumber());
  SequenceNumber sequence_number_info = translator_.get(packet_seq_num, should_skip_packet);
  if (!should_skip_packet && sequence_number_info.type == SequenceNumberType::Valid) {
    rtp_header->setSeqNumber(sequence_number_info.output);
    ELOG_DEBUG("SN %u %d", sequence_number_info.output, is_keyframe);
    last_keyframe_sent_time_ = clock_->now();
    ctx->fireWrite(std::move(packet));
  }
}

bool RtpSlideShowHandler::isVP8OrH264Keyframe(std::shared_ptr<DataPacket> packet) {
  bool is_keyframe = false;
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  uint16_t seq_num = rtp_header->getSeqNumber();
  uint32_t timestamp = rtp_header->getTimestamp();

  if (packet->is_keyframe &&
      (RtpUtils::sequenceNumberLessThan(highest_seq_num_, seq_num) || !highest_seq_num_initialized_)) {
    is_keyframe = true;
    current_keyframe_timestamp_ = timestamp;

    resetKeyframeBuilding();
    is_building_keyframe_ = true;
    first_keyframe_seq_num_ = seq_num;
  } else if (timestamp == current_keyframe_timestamp_) {
    is_keyframe = true;
  }

  return is_keyframe;
}

bool RtpSlideShowHandler::isVP9Keyframe(std::shared_ptr<DataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  uint16_t seq_num = rtp_header->getSeqNumber();
  uint32_t timestamp = rtp_header->getTimestamp();

  if (packet->is_keyframe) {
    if (RtpUtils::sequenceNumberLessThan(seq_num, first_keyframe_seq_num_)) {
      first_keyframe_seq_num_ = seq_num;
      for (uint16_t index = seq_num; index < first_keyframe_seq_num_; index++) {
        keyframe_buffer_.push_back(std::shared_ptr<DataPacket>{});
      }
    }
    if (timestamp != current_keyframe_timestamp_) {
      resetKeyframeBuilding();
      first_keyframe_seq_num_ = seq_num;
      current_keyframe_timestamp_ = timestamp;
      is_building_keyframe_ = true;
    }
  }

  return packet->is_keyframe;
}

void RtpSlideShowHandler::storeKeyframePacket(std::shared_ptr<DataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  uint16_t index = rtp_header->getSeqNumber() - first_keyframe_seq_num_;
  if (index < keyframe_buffer_.size()) {
    keyframe_buffer_[index] = packet;
  }
}

void RtpSlideShowHandler::setSlideShowMode(bool active) {
  if (slideshow_is_active_ == active) {
    return;
  }

  last_keyframe_sent_time_ = clock_->now();
  resetKeyframeBuilding();

  if (active) {
    slideshow_is_active_ = true;
    getContext()->fireRead(RtpUtils::createPLI(stream_->getVideoSinkSSRC(), stream_->getVideoSourceSSRC()));
    stream_->setFeedbackReports(false, 0);
  } else {
    slideshow_is_active_ = false;
    stream_->setFeedbackReports(true, 0);
    getContext()->fireRead(RtpUtils::createPLI(stream_->getVideoSinkSSRC(), stream_->getVideoSourceSSRC()));
  }
}

void RtpSlideShowHandler::maybeUpdateHighestSeqNum(uint16_t seq_num) {
  if (RtpUtils::sequenceNumberLessThan(highest_seq_num_, seq_num) || !highest_seq_num_initialized_) {
    highest_seq_num_ = seq_num;
    highest_seq_num_initialized_ = true;
  }
}

void RtpSlideShowHandler::resetKeyframeBuilding() {
  packets_received_while_building_ = 0;
  first_keyframe_seq_num_ = 0;
  is_building_keyframe_ = false;
  keyframe_buffer_.clear();
  keyframe_buffer_.resize(kMaxKeyframeSize);
}

void RtpSlideShowHandler::consolidateKeyframe() {
  if (packets_received_while_building_++ > kMaxKeyframeSize) {
    resetKeyframeBuilding();
    return;
  }
  std::shared_ptr<DataPacket> packet;
  bool keyframe_complete = false;
  std::vector<std::shared_ptr<DataPacket>> temp_keyframe;

  for (int seq_num = 0; seq_num < kMaxKeyframeSize; seq_num++) {
    packet = keyframe_buffer_[seq_num];
    if (!packet) {
      break;
    }
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    uint32_t timestamp = rtp_header->getTimestamp();

    if (timestamp != current_keyframe_timestamp_) {
      break;
    }

    temp_keyframe.push_back(packet);
    if (rtp_header->getMarker()) {
      //  todo(javier): keyframe may not be complete in VP9 if packet is marker and we have lost packets before
      keyframe_complete = true;
      break;
    }
  }

  if (keyframe_complete) {
    stored_keyframe_.swap(temp_keyframe);
    resetKeyframeBuilding();
    ELOG_DEBUG("Keyframe consolidated");
  }
}

void RtpSlideShowHandler::maybeSendStoredKeyframe() {
  time_point now = clock_->now();
  if (now - last_keyframe_sent_time_ > kFallbackKeyframeTimeout) {
    if (stored_keyframe_.size() > 0) {
      ELOG_DEBUG("Keyframe sent");
    }
    for (auto packet : stored_keyframe_) {
      RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
      rtp_header->setTimestamp(last_timestamp_received_);
      SequenceNumber sequence_number = translator_.generate();
      rtp_header->setSeqNumber(sequence_number.output);
      getContext()->fireWrite(packet);
    }
    last_keyframe_sent_time_ = now;
  }
}
}  // namespace erizo
