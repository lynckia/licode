#ifndef ERIZO_SRC_TEST_UTILS_MATCHERS_H_
#define ERIZO_SRC_TEST_UTILS_MATCHERS_H_

#include <rtp/RtpHeaders.h>
#include <rtp/RtpUtils.h>
#include <MediaDefinitions.h>

namespace erizo {

MATCHER_P(RtpHasPayloadType, pt, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getPayloadType() == pt;
}

MATCHER_P(RtpHasSsrc, ssrc, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getSSRC() == ssrc;
}

MATCHER_P(RtpHasSequenceNumberFromBuffer, seq_num, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)))->getSeqNumber() == seq_num;
}

MATCHER_P(RtpHasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg)->data))->getSeqNumber() == seq_num;
}
MATCHER_P(NackHasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getNackPid() == seq_num;
}
MATCHER_P(ReceiverReportHasSequenceNumber, seq_num, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getHighestSeqnum() == seq_num;
}
MATCHER_P(ReceiverReportHasSeqnumCycles, cycles, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getSeqnumCycles() == cycles;
}
MATCHER_P(ReceiverReportHasLostPacketsValue, lost_packets, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getLostPackets() == lost_packets;
}
MATCHER_P(ReceiverReportHasFractionLostValue, lost_fraction, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getFractionLost() == lost_fraction;
}
MATCHER_P(ReceiverReportHasDlsrValue, delay_since_last_sr, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getDelaySinceLastSr() == delay_since_last_sr;
}
MATCHER_P(SenderReportHasPacketsSentValue, packets_sent, "") {
  uint unsigned_packets_sent = abs(packets_sent);
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getPacketsSent() == unsigned_packets_sent;
}
MATCHER_P(SenderReportHasOctetsSentValue, octets_sent, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getOctetsSent() == octets_sent;
}
MATCHER_P(RembHasBitrateValue, bitrate, "") {
  return (reinterpret_cast<erizo::RtcpHeader*>(std::get<0>(arg)->data))->getREMBBitRate() == bitrate;
}

MATCHER(IsPLI, "") {
  auto packet = std::get<0>(arg);
  return RtpUtils::isPLI(packet);
}

MATCHER(IsFIR, "") {
  auto packet = std::get<0>(arg);
  return RtpUtils::isFIR(packet);
}

MATCHER_P(PacketLengthIs, packet_length, "") {
  return (std::get<0>(arg)->length == packet_length);
}
MATCHER_P(PacketBelongsToSpatialLayer, spatial_layer_id, "") {
  return std::get<0>(arg)->belongsToSpatialLayer(spatial_layer_id);
}
MATCHER_P(PacketBelongsToTemporalLayer, temporal_layer_id, "") {
  return std::get<0>(arg)->belongsToTemporalLayer(temporal_layer_id);
}
MATCHER_P(PacketDoesNotBelongToSpatialLayer, spatial_layer_id, "") {
  return !std::get<0>(arg)->belongsToSpatialLayer(spatial_layer_id);
}
MATCHER_P(PacketDoesNotBelongToTemporalLayer, temporal_layer_id, "") {
  return !std::get<0>(arg)->belongsToTemporalLayer(temporal_layer_id);
}
MATCHER(PacketIsKeyframe, "") {
  return std::get<0>(arg)->is_keyframe;
}
MATCHER(PacketIsNotKeyframe, "") {
  return !std::get<0>(arg)->is_keyframe;
}

MATCHER(IsKeyframeFirstPacket, "") {
  erizo::RtpHeader *packet = reinterpret_cast<erizo::RtpHeader*>(std::get<0>(arg));
  char* data_pointer;
  char* parsing_pointer;
  data_pointer = std::get<0>(arg) + packet->getHeaderLength();
  parsing_pointer = data_pointer;
  if (*parsing_pointer != 0x10) {
    return false;
  }
  parsing_pointer++;
  if (*parsing_pointer == 0x00) {
    return true;
  }
  return false;
}

MATCHER_P(ConnectionQualityEventWithLevel, level, "") {
  auto media_event = std::get<0>(arg);
  auto quality_event = std::static_pointer_cast<ConnectionQualityEvent>(media_event);
  return quality_event->level == level;
}

}  // namespace erizo

#endif  // ERIZO_SRC_TEST_UTILS_MATCHERS_H_
