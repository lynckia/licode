#include "./MediaDefinitions.h"
#include "rtp/RtpSlideShowHandler.h"

namespace erizo {

DEFINE_LOGGER(RtpSlideShowHandler, "rtp.RtpSlideShowHandler");

RtpSlideShowHandler::RtpSlideShowHandler()
  : connection_{nullptr},
    slideshow_seq_num_{-1}, last_original_seq_num_{-1}, seq_num_offset_{0},
    slideshow_is_active_{false},
    sending_keyframe_ {false} {}


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
  if (seq_num_offset_ > 0) {
    char* buf = packet->data;
    char* report_pointer = buf;
    int rtcp_length = 0;
    int total_length = 0;
    do {
      report_pointer += rtcp_length;
      chead = reinterpret_cast<RtcpHeader*>(report_pointer);
      rtcp_length = (ntohs(chead->length) + 1) * 4;
      total_length += rtcp_length;
      switch (chead->packettype) {
        case RTCP_Receiver_PT:
          if ((chead->getHighestSeqnum() + seq_num_offset_) < chead->getHighestSeqnum()) {
            // The seqNo adjustment causes a wraparound, add to cycles
            chead->setSeqnumCycles(chead->getSeqnumCycles() + 1);
          }
          chead->setHighestSeqnum(chead->getHighestSeqnum() + seq_num_offset_);

          break;
        case RTCP_RTP_Feedback_PT:
          chead->setNackPid(chead->getNackPid() + seq_num_offset_);
          break;
        default:
          break;
      }
    } while (total_length < packet->length);
  }
  ctx->fireRead(packet);
}

void RtpSlideShowHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);

  if (packet->type != VIDEO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireWrite(packet);
    return;
  }
  last_original_seq_num_ = rtp_header->getSeqNumber();
  if (slideshow_seq_num_ == -1) {  // We didn't receive any packets before setting up slideshow
    slideshow_seq_num_ = last_original_seq_num_;
  }
  if (slideshow_is_active_) {
    bool is_keyframe = false;
    RtpMap *codec = connection_->getRemoteSdpInfo().getCodecByExternalPayloadType(rtp_header->getPayloadType());
    if (codec && codec->encoding_name == "VP8") {
      is_keyframe = isVP8Keyframe(packet);
    } else if (codec && codec->encoding_name == "VP9") {
      is_keyframe = isVP9Keyframe(packet);
    }
    if (is_keyframe) {
      setPacketSeqNumber(packet, slideshow_seq_num_++);
      ctx->fireWrite(packet);
    }
  } else {
    if (seq_num_offset_ > 0) {
      setPacketSeqNumber(packet, (last_original_seq_num_ - seq_num_offset_));
    }
    ctx->fireWrite(packet);
  }
}

bool RtpSlideShowHandler::isVP8Keyframe(std::shared_ptr<dataPacket> packet) {
  bool is_keyframe = false;
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadVP8* payload = vp8_parser_.parseVP8(
      start_buffer, packet->length - rtp_header->getHeaderLength());
  if (!payload->frameType) {  // Its a keyframe first packet
    sending_keyframe_ = true;
  }
  delete payload;
  is_keyframe = sending_keyframe_;
  if (sending_keyframe_ && rtp_header->getMarker()) {
    sending_keyframe_ = false;
  }
  return is_keyframe;
}

bool RtpSlideShowHandler::isVP9Keyframe(std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
  start_buffer = start_buffer + rtp_header->getHeaderLength();
  RTPPayloadVP9* payload = vp9_parser_.parseVP9(
      start_buffer, packet->length - rtp_header->getHeaderLength());
  return !payload->frameType;
}

void RtpSlideShowHandler::setSlideShowMode(bool active) {
  if (slideshow_is_active_ == active) {
    return;
  }

  if (active) {
    slideshow_seq_num_ = last_original_seq_num_ - seq_num_offset_;
    sending_keyframe_ = false;
    slideshow_is_active_ = true;
    connection_->setFeedbackReports(false, 0);
    ELOG_DEBUG("%s message: Setting seqNo, seqNo: %u", connection_->toLog(), slideshow_seq_num_);
  } else {
    seq_num_offset_ = last_original_seq_num_ - slideshow_seq_num_ + 1;
    ELOG_DEBUG("%s message: Changing offset manually, original_seq_num: %u, slideshow_seq_num: %u, offset: %u",
                connection_->toLog(), last_original_seq_num_, slideshow_seq_num_, seq_num_offset_);
    slideshow_is_active_ = false;
    connection_->setFeedbackReports(true, 0);
  }
}

inline void RtpSlideShowHandler::setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number) {
  RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    return;
  }
  head->setSeqNumber(seq_number);
}

}  // namespace erizo
