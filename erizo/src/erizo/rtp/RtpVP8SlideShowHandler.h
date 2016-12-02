#ifndef ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_


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
  WebRtcConnection *connection_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_
