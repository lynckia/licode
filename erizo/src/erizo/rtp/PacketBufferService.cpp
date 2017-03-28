#include "rtp/PacketBufferService.h"

namespace erizo {
DEFINE_LOGGER(PacketBufferService, "rtp.PacketBufferService");

PacketBufferService::PacketBufferService(): audio_{kServicePacketBufferSize},
  video_{kServicePacketBufferSize} {
  ELOG_DEBUG("Constructor, video size %u", video_.size());
}

void PacketBufferService::insertPacket(std::shared_ptr<dataPacket> packet) {
  RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
  switch (packet->type) {
    case VIDEO_PACKET:
    ELOG_DEBUG("Trying to insert video packet %u, result %u, size %u", head->getSeqNumber(),
        getIndexInBuffer(head->getSeqNumber()), video_.size());
      video_[getIndexInBuffer(head->getSeqNumber())] = packet;
      break;
    case AUDIO_PACKET:
    ELOG_DEBUG("Trying to insert audio packet %u, result %u", head->getSeqNumber(),
        getIndexInBuffer(head->getSeqNumber()));
      audio_[getIndexInBuffer(head->getSeqNumber())] = packet;
      break;
    default:
      ELOG_INFO("message: Trying to store an unknown packet");
      break;
  }
}

std::shared_ptr<dataPacket> PacketBufferService::getVideoPacket(uint16_t seq_num) {
  return video_[getIndexInBuffer(seq_num)];
}
std::shared_ptr<dataPacket> PacketBufferService::getAudioPacket(uint16_t seq_num) {
  return audio_[getIndexInBuffer(seq_num)];
}
uint16_t PacketBufferService::getIndexInBuffer(uint16_t seq_num) {
  return seq_num % kServicePacketBufferSize;
}

}  // namespace erizo
