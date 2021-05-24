#ifndef ERIZO_SRC_ERIZO_HANDLERS_HANDLERS_LOGGERHANDLER_H_
#define ERIZO_SRC_ERIZO_HANDLERS_HANDLERS_LOGGERHANDLER_H_

#include "../../pipeline/Handler.h"  // Import CustomHandler interface
#include "../../MediaDefinitions.h"  // Imports DataPacket struct
#include "./logger.h"  // Include logger


namespace erizo {  // Handlers are include in erizo namespace
class LoggerHandler : public CustomHandler {
  DECLARE_LOGGER();  // Declares logger for debugging and logging
 public:
    explicit LoggerHandler(std::map <std::string, std::string> parameters);
    ~LoggerHandler();
    void read(Context *ctx, std::shared_ptr <DataPacket> packet) override;  // Process packet sent by client
    void write(Context *ctx, std::shared_ptr <DataPacket> packet) override;  // Process packet sent to client
    Positions position() override;  // Returns position to place handler.
    void enable() override;  // Enable handler
    void disable() override;  // Disable handler
    std::string getName() override;  // Returns handler name
    void notifyUpdate() override;  // Recieves update
 private:
    RtcpHeader* rtcp_head;
    RtpHeader* rtp_head;
    bool is_enabled = true;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_HANDLERS_HANDLERS_LOGGERHANDLER_H_"
