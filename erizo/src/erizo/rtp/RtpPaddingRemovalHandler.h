
#ifndef ERIZO_SRC_ERIZO_RTP_RTPPADDINGREMOVALHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPPADDINGREMOVALHANDLER_H_

#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/SequenceNumberTranslator.h"

namespace erizo {

class MediaStream;

class RtpPaddingRemovalHandler: public Handler, public std::enable_shared_from_this<RtpPaddingRemovalHandler> {
  DECLARE_LOGGER();


 public:
  RtpPaddingRemovalHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "padding_removal";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  bool removePaddingBytes(std::shared_ptr<DataPacket> packet,
      std::shared_ptr<SequenceNumberTranslator> translator);
  std::shared_ptr<SequenceNumberTranslator> getTranslatorForSsrc(uint32_t ssrc,
    bool should_create);

 private:
  bool enabled_;
  bool initialized_;
  std::map<uint32_t, std::shared_ptr<SequenceNumberTranslator>> translator_map_;
  MediaStream* stream_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPPADDINGREMOVALHANDLER_H_
