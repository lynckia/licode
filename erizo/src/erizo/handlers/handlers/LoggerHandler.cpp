
#include "LoggerHandler.h"

#include "../../rtp/RtpHeaders.h"  // Imports headers utilities

namespace erizo {

  DEFINE_LOGGER(LoggerHandler, "rtp.LoggerHandler");  // Defines handler logger name

  LoggerHandler::LoggerHandler(std::map <std::string, std::string> parameters) {
    ELOG_DEBUG("Creating Logger Handler ");
  }


  LoggerHandler::~LoggerHandler() {
    ELOG_DEBUG("Destroying Logger Handler ");
  }

  Positions LoggerHandler::position() {
    return MIDDLE;
  }

  void LoggerHandler::write(Context *ctx, std::shared_ptr <DataPacket> packet) {
    if (is_enabled) {
      ELOG_DEBUG("------------------------");
      ELOG_DEBUG("Writing packet");
      ELOG_DEBUG("Packet length %d", packet->length);
      rtcp_head = reinterpret_cast<RtcpHeader*> (packet->data);  // Read data as RTCP packet
      if (rtcp_head->isRtcp()) {
          ELOG_DEBUG("Rtcp packet received");
      } else if (packet->type == VIDEO_PACKET) {
          rtp_head = reinterpret_cast<RtpHeader*> (packet->data);  // Read RTP data and log some fields
          ELOG_DEBUG("Rtp video packet received");
          ELOG_DEBUG("Timestamp %d",  rtp_head->timestamp);
          ELOG_DEBUG("SSRC %d",  rtp_head->ssrc);
      } else if (packet->type == AUDIO_PACKET) {  // Audio packets
          ELOG_DEBUG("Audio packet received");
      } else if (packet->type == OTHER_PACKET) {  // Data or other packets
          ELOG_DEBUG("Other packet received");
      }
      if (packet->priority == HIGH_PRIORITY) {  // Log priority field for packet
          ELOG_DEBUG("Packet with high priority");
      } else if (packet->priority == LOW_PRIORITY) {
          ELOG_DEBUG("Packet with low priority");
      }
      ctx->fireWrite(std::move(packet));  // Gives packet ownership to next handler in pipeline
    }
  }

  void LoggerHandler::read(Context *ctx, std::shared_ptr <DataPacket> packet) {
    if (is_enabled) {
        ELOG_DEBUG("------------------------");
        ELOG_DEBUG("Reading packet ");
        ELOG_DEBUG("Packet length %d", packet->length);
        rtcp_head = reinterpret_cast<RtcpHeader*> (packet->data);
        if (rtcp_head->isRtcp()) {
            ELOG_DEBUG("Rtcp packet received");
        } else if (packet->type == VIDEO_PACKET) {
            rtp_head = reinterpret_cast<RtpHeader*> (packet->data);
            ELOG_DEBUG("Rtp video packet received");
            ELOG_DEBUG("Timestamp %d",  rtp_head->timestamp);
            ELOG_DEBUG("SSRC %d",  rtp_head->ssrc);
        } else if (packet->type == AUDIO_PACKET) {
            ELOG_DEBUG("Audio packet received");
        } else if (packet->type == OTHER_PACKET) {
            ELOG_DEBUG("Other packet received");
        }
        if (packet->priority == HIGH_PRIORITY) {
            ELOG_DEBUG("Packet with high priority");
        } else if (packet->priority == LOW_PRIORITY) {
            ELOG_DEBUG("Packet with low priority");
        }
        ELOG_DEBUG("------------------------");
        ctx->fireRead(std::move(packet));  // Gives packet ownership to next handler in pipeline
    }
  }

  void LoggerHandler::enable() {
      is_enabled = true;
  }

  void LoggerHandler::disable() {
    is_enabled = false;
  }

  std::string LoggerHandler::getName() {
      return "LoggerHandler";
  }

  void LoggerHandler::notifyUpdate() {
  }
}  // namespace erizo
