#ifndef ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_

#include <boost/thread/mutex.hpp>
#include <string>

#include "pipeline/Handler.h"
#include "./logger.h"
#include "./WebRtcConnection.h"
#include "rtp/RtpSlideShowHandler.h"

namespace erizo {
class RtpVP8SlideShowHandler : public RtpSlideShowHandler {
  DECLARE_LOGGER();

 public:
  explicit RtpVP8SlideShowHandler(WebRtcConnection* connection);
  void setSlideShowMode(bool activated) override;

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "slideshow-vp8";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;

 private:
  int32_t slideshow_seq_num_, last_original_seq_num_;
  uint16_t seq_num_offset_;

  bool slideshow_is_active_, sending_keyframe_;
  boost::mutex slideshow_mutex_;

  inline void setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number);
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPVP8SLIDESHOWHANDLER_H_
