//
// Created by lic on 25/11/20.
//

#ifndef ERIZO_SRC_ERIZO_RTP_LOWERFPSHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_LOWERFPSHANDLER_H_


#include "../pipeline/Handler.h"
#include "./logger.h"



namespace erizo {

class MediaStream;

class LowerFPSHandler : public CustomHandler {
    DECLARE_LOGGER();

public:
    LowerFPSHandler(  std::map <std::string, std::string> parameters );
    ~LowerFPSHandler(){};

    void enable() override;
    void disable() override;

    std::string getName() override {
        return "slideshow";
    }

    void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
    void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
    void notifyUpdate() override;

    Positions position () override;

private:
    std::map <std::string, std::string> parameters;

};

}
#endif  // ERIZO_SRC_ERIZO_RTP_LOWERFPSHANDLER_H_
