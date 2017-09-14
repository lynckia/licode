#ifndef ERIZO_SRC_ERIZO_RTP_PACKETBUFFERSERVICE_H_
#define ERIZO_SRC_ERIZO_RTP_PACKETBUFFERSERVICE_H_

#include "./logger.h"
#include "./MediaDefinitions.h"
#include "rtp/RtpHeaders.h"
#include "pipeline/Service.h"

static constexpr uint16_t kServicePacketBufferSize = 256;

namespace erizo {
class PacketBufferService: public Service {
 public:
  DECLARE_LOGGER();

  PacketBufferService();
  ~PacketBufferService() {}

  PacketBufferService(const PacketBufferService&& service);

  void insertPacket(std::shared_ptr<DataPacket> packet);

  std::shared_ptr<DataPacket> getVideoPacket(uint16_t seq_num);
  std::shared_ptr<DataPacket> getAudioPacket(uint16_t seq_num);

 private:
  uint16_t getIndexInBuffer(uint16_t seq_num);

 private:
  std::vector<std::shared_ptr<DataPacket>> audio_;
  std::vector<std::shared_ptr<DataPacket>> video_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_RTP_PACKETBUFFERSERVICE_H_
