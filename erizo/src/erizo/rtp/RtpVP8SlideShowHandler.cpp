#include "rtp/RtpVP8SlideShowHandler.h"

namespace erizo {

DEFINE_LOGGER(RtpVP8SlideShowHandler, "rtp.RtpVP8SlideShowHandler");

RtpVP8SlideShowHandler::RtpVP8SlideShowHandler(WebRtcConnection *connection) : RtpSlideShowHandler(connection) {
}

void RtpVP8SlideShowHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  ELOG_DEBUG("Read packet in slideshow filter length: %u", packet->length);
  ctx->fireRead(packet);
}

void RtpVP8SlideShowHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  ELOG_DEBUG("Write packet in slideshow filter length: %u", packet->length);
  ctx->fireWrite(packet);
}

void RtpVP8SlideShowHandler::setSlideShowMode(bool active) {
  /*
  ELOG_DEBUG("setting slideshowMode %d", active);
  if (slideShowMode_ == state) {
    return;
  }
  if (state == true) {
    seqNo_ = sendSeqNo_ - seqNoOffset_;
    grace_ = 0;
    slideShowMode_ = true;
    shouldSendFeedback_ = false;
    ELOG_DEBUG("%s message: Setting seqNo, seqNo: %u", toLog(), seqNo_);
  } else {
    seqNoOffset_ = sendSeqNo_ - seqNo_ + 1;
    ELOG_DEBUG("%s message: Changing offset manually, sendSeqNo: %u, seqNo: %u, offset: %u",
                toLog(), sendSeqNo_, seqNo_, seqNoOffset_);
    slideShowMode_ = false;
    shouldSendFeedback_ = true;
  }
  */
}

}  // namespace erizo
