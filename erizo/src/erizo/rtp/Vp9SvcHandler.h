#ifndef ERIZO_SRC_ERIZO_RTP_VP9SVCHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_VP9SVCHANDLER_H_

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/RtpVP9Parser.h"

#include <mutex>  // NOLINT

namespace erizo {

class WebRtcConnection;

class Vp9SvcHandler: public Handler {
  DECLARE_LOGGER();

 public:
  explicit Vp9SvcHandler(WebRtcConnection* connection);

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "audio-mute";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

  void logPayload(RTPPayloadVP9 *payload);

 private:
  int32_t  last_sent_seq_num_;
  uint16_t seq_num_offset_;

  bool mute_is_active_;

  int temporal_id_;
  int spatial_id_;

  WebRtcConnection* connection_;

  inline void setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_VP9SVCHANDLER_H_
