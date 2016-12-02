#ifndef ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_

#include <boost/thread/mutex.hpp>

#include "pipeline/Handler.h"
#include "./logger.h"
#include "./WebRtcConnection.h"
#include "rtp/RtpSlideShowHandler.h"

namespace erizo {
class RtpVP8SlideShowHandler : public RtpSlideShowHandler {
  DECLARE_LOGGER();

 public:
  explicit RtpVP8SlideShowHandler(WebRtcConnection* connection);
  void setSlideShowMode(bool activated);

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;


 private:
  uint16_t seqNo_, grace_, sendSeqNo_, seqNoOffset_;
  bool slideshow_is_active_;
  boost::mutex slideshow_mutex_;

  inline void setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number);
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_
