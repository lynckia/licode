#ifndef ERIZO_SRC_ERIZO_RTP_RTXPACKETTRANSLATOR_H_
#define ERIZO_SRC_ERIZO_RTP_RTXPACKETTRANSLATOR_H_

#include "./logger.h"
#include "MediaStream.h"
#include "pipeline/Handler.h"

namespace erizo {
class MediaStream;

class RtxPacketTranslator : public InboundHandler {
    DECLARE_LOGGER();

 public:
    void enable() override {}
    void disable() override {}

    std::string getName() override {
        return "rtx_packet_translator";
    }

    void read(Context* ctx, std::shared_ptr<DataPacket> packet) override;
    void notifyUpdate() override;

 private:
    std::map<uint32_t, uint32_t> fid_map;
    std::map<unsigned, unsigned> apt_translator;
    bool initialized_;
    MediaStream* stream_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTXPACKETTRANSLATOR_H_
