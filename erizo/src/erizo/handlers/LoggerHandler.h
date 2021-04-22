#ifndef ERIZO_SRC_ERIZO_HANDLERS_LOGGERHANDLER_H_
#define ERIZO_SRC_ERIZO_HANDLERS_LOGGERHANDLER_H_

#include "../pipeline/Handler.h"  // Import CustomHandler interface
#include "../MediaDefinitions.h"  // Imports DataPacket struct
#include "./logger.h"  // Include logger


namespace erizo { // Handlers are include in erizo namespace
    class LoggerHandler : public CustomHandler {
        DECLARE_LOGGER(); // Declares logger for debugging and logging
    public:
        LoggerHandler(std::map <std::string, std::string> parameters);

        ~LoggerHandler();

        void enable() override; // When called disables the handler

        void disable() override;  // When called enables handler

        std::string getName() override {  //Function implemented by handlers
            return "LoggerHandler";
        }

        void read(Context *ctx, std::shared_ptr <DataPacket> packet) override;  // Process packet sent by client
        void write(Context *ctx, std::shared_ptr <DataPacket> packet) override;  // Process packet sent to client
        void notifyUpdate() override;  // Receives notifications and updates handle
        Positions position() override;  // Returns position to place handler.
    private:
        RtcpHeader* rtcp_head;
        RtpHeader* rtp_head;
    };
}

#endif  // ERIZO_SRC_ERIZO_HANDLERS_LOGGERHANDLER_H_
