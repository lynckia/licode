#include "rtp/LowerFPSHandler.h"
#include "MediaStream.h"
#include <vector>

#include "./MediaDefinitions.h"
#include "rtp/RtpUtils.h"



namespace erizo {


DEFINE_LOGGER(LowerFPSHandler, "rtp.LowerFPSHandler");

    void LowerFPSHandler::enable() {
    }

    void LowerFPSHandler::disable() {
    }

    void LowerFPSHandler::notifyUpdate() {

    }

    int LowerFPSHandler::position(){
        return 1;
    }

    void LowerFPSHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
        if (packet->type != VIDEO_PACKET){
            ctx->fireWrite(std::move(packet));
        }
    }

    void LowerFPSHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
        if (packet->type != VIDEO_PACKET){
             ctx->fireWrite(std::move(packet));
        }

    }

}