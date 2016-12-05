#include "./MediaDefinitions.h"
#include "rtp/RtpVP8SlideShowHandler.h"
#include "rtp/RtpVP8Parser.h"

namespace erizo {

DEFINE_LOGGER(RtpVP8SlideShowHandler, "rtp.RtpVP8SlideShowHandler");

RtpVP8SlideShowHandler::RtpVP8SlideShowHandler(WebRtcConnection *connection) : RtpSlideShowHandler(connection),
  slideshow_seq_num_{-1}, last_original_seq_num_{-1}, seq_num_offset_{0}, slideshow_is_active_{false},
  sending_keyframe_ {false} {}

void RtpVP8SlideShowHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (connection_->getVideoSinkSSRC() != chead->getSourceSSRC()) {
    ctx->fireRead(packet);
    return;
  }
  slideshow_mutex_.lock();
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
  slideshow_mutex_.unlock();
  ctx->fireRead(packet);
}

void RtpVP8SlideShowHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);

  if (packet->type != VIDEO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireWrite(packet);
    return;
  }
  slideshow_mutex_.lock();
  last_original_seq_num_ = rtp_header->getSeqNumber();
  if (slideshow_seq_num_ == -1) {  // We didn't receive any packets before setting up slideshow
    slideshow_seq_num_ = last_original_seq_num_;
  }
  if (slideshow_is_active_) {
    RtpVP8Parser parser;
    unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    RTPPayloadVP8* payload = parser.parseVP8(
        start_buffer, packet->length - rtp_header->getHeaderLength());
    if (!payload->frameType) {  // Its a keyframe
      sending_keyframe_ = true;
    }
    delete payload;
    if (sending_keyframe_) {  // We send until marker
      setPacketSeqNumber(packet, slideshow_seq_num_++);
      slideshow_mutex_.unlock();
      ctx->fireWrite(packet);
      if (rtp_header->getMarker()) {
        sending_keyframe_ = false;
      }
    } else {
      slideshow_mutex_.unlock();
    }
  } else {
    if (seq_num_offset_ > 0) {
      setPacketSeqNumber(packet, (last_original_seq_num_ - seq_num_offset_));
    }
    slideshow_mutex_.unlock();
    ctx->fireWrite(packet);
  }
}

void RtpVP8SlideShowHandler::setSlideShowMode(bool active) {
  boost::mutex::scoped_lock lock(slideshow_mutex_);
  if (slideshow_is_active_ == active) {
    return;
  }

  if (active) {
    slideshow_seq_num_ = last_original_seq_num_ - seq_num_offset_;
    sending_keyframe_ = 0;
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

inline void RtpVP8SlideShowHandler::setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number) {
  RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    return;
  }
  head->setSeqNumber(seq_number);
}

}  // namespace erizo
