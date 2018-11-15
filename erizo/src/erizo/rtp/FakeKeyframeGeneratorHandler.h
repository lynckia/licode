#ifndef ERIZO_SRC_ERIZO_RTP_FAKEKEYFRAMEGENERATORHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_FAKEKEYFRAMEGENERATORHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "lib/Clock.h"
#include "lib/TokenBucket.h"
#include "thread/Worker.h"
#include "rtp/SequenceNumberTranslator.h"
#include "rtp/RtpVP8Parser.h"
#include "./Stats.h"

namespace erizo {

class MediaStream;

class FakeKeyframeGeneratorHandler: public Handler, public std::enable_shared_from_this<FakeKeyframeGeneratorHandler> {
  DECLARE_LOGGER();

 public:
  FakeKeyframeGeneratorHandler();
  void enable() override;
  void disable() override;

  std::string getName() override {
    return "fake-keyframe-generator";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
  void notifyUpdate() override;

 private:
  std::shared_ptr<DataPacket> transformIntoKeyframePacket(std::shared_ptr<DataPacket> packet);
  void maybeSendAndSchedulePLIs();
  void sendPLI();
  void schedulePLI();

 private:
  MediaStream* stream_;
  bool enabled_;
  bool first_keyframe_received_;
  bool plis_scheduled_;
  uint32_t video_source_ssrc_;
  uint32_t video_sink_ssrc_;
  RtpVP8Parser vp8_parser_;
  std::shared_ptr<ScheduledTaskReference> scheduled_task_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_FAKEKEYFRAMEGENERATORHANDLER_H_
