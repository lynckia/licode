#ifndef ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_

#include <string>

#include "pipeline/Handler.h"
#include "./logger.h"
#include "./WebRtcConnection.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtpVP9Parser.h"

namespace erizo {
class RtpSlideShowHandler : public Handler {
  DECLARE_LOGGER();

 public:
  RtpSlideShowHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "slideshow";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

  void setSlideShowMode(bool activated);

 private:
  bool isVP8Keyframe(std::shared_ptr<dataPacket> packet);
  bool isVP9Keyframe(std::shared_ptr<dataPacket> packet);

 private:
  WebRtcConnection* connection_;
  int32_t slideshow_seq_num_, last_original_seq_num_;
  uint16_t seq_num_offset_;

  bool slideshow_is_active_, sending_keyframe_;
  RtpVP8Parser vp8_parser_;
  RtpVP9Parser vp9_parser_;

  inline void setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number);
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_
