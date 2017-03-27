#ifndef ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_

#include <string>

#include "pipeline/Handler.h"
#include "./logger.h"
#include "./WebRtcConnection.h"
#include "rtp/SequenceNumberTranslator.h"
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
  void maybeUpdateHighestSeqNum(uint16_t seq_num);

 private:
  WebRtcConnection* connection_;
  SequenceNumberTranslator translator_;
  bool highest_seq_num_initialized_;
  uint16_t  highest_seq_num_;

  bool slideshow_is_active_;
  uint32_t current_keyframe_timestamp_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPSLIDESHOWHANDLER_H_
