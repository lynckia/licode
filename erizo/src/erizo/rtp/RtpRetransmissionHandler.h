#ifndef ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_

#include <boost/thread/mutex.hpp>
#include <vector>

#include "pipeline/Handler.h"

#include "./WebRtcConnection.h"

static constexpr uint kRetransmissionsBufferSize = 256;
static constexpr int kNackBlpSize = 16;

namespace erizo {
class RtpRetransmissionHandler : public Handler {
 public:
  DECLARE_LOGGER();

  explicit RtpRetransmissionHandler(WebRtcConnection *connection);

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;

  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;

 private:
  uint16_t getIndexInBuffer(uint16_t seq_num);

 private:
  WebRtcConnection *connection_;
  std::vector<std::shared_ptr<dataPacket>> audio_;
  std::vector<std::shared_ptr<dataPacket>> video_;
  std::shared_ptr<boost::mutex> buffer_mutex_ptr_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPRETRANSMISSIONHANDLER_H_
