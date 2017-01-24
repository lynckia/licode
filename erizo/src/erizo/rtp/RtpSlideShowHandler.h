#ifndef ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_

#include <string>

#include "pipeline/Handler.h"
#include "./WebRtcConnection.h"

namespace erizo {
class RtpSlideShowHandler: public Handler {
 public:
  explicit RtpSlideShowHandler(WebRtcConnection* connection) : connection_{connection} {};
  virtual void setSlideShowMode(bool activated) = 0;

  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual std::string getName() = 0;

  virtual void read(Context *ctx, std::shared_ptr<dataPacket> packet) = 0;
  virtual void write(Context *ctx, std::shared_ptr<dataPacket> packet) = 0;

 protected:
  WebRtcConnection *connection_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_
