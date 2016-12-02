#include "./MediaDefinitions.h"
#include "rtp/RtpVP8SlideShowHandler.h"
#include "rtp/RtpVP8Parser.h"

namespace erizo {

DEFINE_LOGGER(RtpVP8SlideShowHandler, "rtp.RtpVP8SlideShowHandler");

RtpVP8SlideShowHandler::RtpVP8SlideShowHandler(WebRtcConnection *connection) : RtpSlideShowHandler(connection) {
  seqNo_ = 1000;
  sendSeqNo_ = 0;
  grace_ = 0;
  seqNoOffset_ = 0;
  slideshow_is_active_ = false;
}

void RtpVP8SlideShowHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  boost::mutex::scoped_lock lock(slideshow_mutex_);
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (connection_->getVideoSinkSSRC() != chead->getSourceSSRC()) {
    ctx->fireRead(packet);
    return;
  }
  if (seqNoOffset_ > 0) {
    char* buf = packet->data;
    char* movingBuf = buf;
    int rtcpLength = 0;
    int totalLength = 0;
    do {
      movingBuf += rtcpLength;
      chead = reinterpret_cast<RtcpHeader*>(movingBuf);
      rtcpLength = (ntohs(chead->length) + 1) * 4;
      totalLength += rtcpLength;
      switch (chead->packettype) {
        case RTCP_Receiver_PT:
          if ((chead->getHighestSeqnum() + seqNoOffset_) < chead->getHighestSeqnum()) {
            // The seqNo adjustment causes a wraparound, add to cycles
            chead->setSeqnumCycles(chead->getSeqnumCycles() + 1);
          }
          chead->setHighestSeqnum(chead->getHighestSeqnum() + seqNoOffset_);

          break;
        case RTCP_RTP_Feedback_PT:
          chead->setNackPid(chead->getNackPid() + seqNoOffset_);
          break;
        default:
          break;
      }
    } while (totalLength < packet->length);
  }
  ctx->fireRead(packet);
}

void RtpVP8SlideShowHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  boost::mutex::scoped_lock lock(slideshow_mutex_);
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  RtcpHeader *rtcp_header = reinterpret_cast<RtcpHeader*>(packet->data);

  if (packet->type != VIDEO_PACKET || rtcp_header->isRtcp()) {
    ctx->fireWrite(packet);
    return;
  }
  sendSeqNo_ = rtp_header->getSeqNumber();
  if (slideshow_is_active_) {
    RtpVP8Parser parser;
    unsigned char* start_buffer = reinterpret_cast<unsigned char*> (packet->data);
    start_buffer = start_buffer + rtp_header->getHeaderLength();
    RTPPayloadVP8* payload = parser.parseVP8(
        start_buffer, packet->length - rtp_header->getHeaderLength());
    if (!payload->frameType) {  // Its a keyframe
      grace_ = 1;
    }
    delete payload;
    if (grace_) {  // We send until marker
      setPacketSeqNumber(packet, seqNo_++);
      ctx->fireWrite(packet);
      if (rtp_header->getMarker()) {
        grace_ = 0;
      }
    }
  } else {
    if (seqNoOffset_ > 0) {
      setPacketSeqNumber(packet, (sendSeqNo_ - seqNoOffset_));
    }
    ctx->fireWrite(packet);
  }
}

void RtpVP8SlideShowHandler::setSlideShowMode(bool active) {
  boost::mutex::scoped_lock lock(slideshow_mutex_);
  if (slideshow_is_active_ == active) {
    return;
  }

  if (active) {
    seqNo_ = sendSeqNo_ - seqNoOffset_;
    grace_ = 0;
    slideshow_is_active_ = true;
    connection_->setFeedbackReports(false, 0);
    ELOG_DEBUG("%s message: Setting seqNo, seqNo: %u", connection_->toLog(), seqNo_);
  } else {
    seqNoOffset_ = sendSeqNo_ - seqNo_ + 1;
    ELOG_DEBUG("%s message: Changing offset manually, sendSeqNo: %u, seqNo: %u, offset: %u",
                connection_->toLog(), sendSeqNo_, seqNo_, seqNoOffset_);
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
