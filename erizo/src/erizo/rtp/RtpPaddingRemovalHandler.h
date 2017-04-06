#ifndef ERIZO_SRC_ERIZO_RTP_RTPPADDINGREMOVALHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPPADDINGREMOVALHANDLER_H_

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/SequenceNumberTranslator.h"

namespace erizo {

class WebRtcConnection;

class RtpPaddingRemovalHandler: public Handler, public std::enable_shared_from_this<RtpPaddingRemovalHandler> {
  DECLARE_LOGGER();


 public:
  RtpPaddingRemovalHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "padding_removal";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void removePaddingBytes(std::shared_ptr<dataPacket> packet);

 private:
  bool enabled_;
  bool initialized_;
  SequenceNumberTranslator translator_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPPADDINGREMOVALHANDLER_H_
