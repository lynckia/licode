#include "rtp/RtpPaddingRemovalHandler.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(RtpPaddingRemovalHandler, "rtp.RtpPaddingRemovalHandler");

RtpPaddingRemovalHandler::RtpPaddingRemovalHandler()
  : enabled_{true}, initialized_{false} {}

void RtpPaddingRemovalHandler::enable() {
  enabled_ = true;
}

void RtpPaddingRemovalHandler::disable() {
  enabled_ = false;
}

void RtpPaddingRemovalHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);

  if (!chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    uint32_t ssrc = rtp_header->getSSRC();
    std::shared_ptr<SequenceNumberTranslator> translator = getTranslatorForSsrc(ssrc, true);
    if (!removePaddingBytes(packet, translator)) {
      return;
    }
    uint16_t sequence_number = rtp_header->getSeqNumber();
    SequenceNumber sequence_number_info = translator->get(sequence_number, false);

    if (sequence_number_info.type != SequenceNumberType::Valid) {
      return;
    }
    rtp_header->setSeqNumber(sequence_number_info.output);
  }
  ctx->fireRead(packet);
}

void RtpPaddingRemovalHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader* rtcp_head = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!rtcp_head->isFeedback()) {
    ctx->fireWrite(packet);
    return;
  }
  uint32_t ssrc = rtcp_head->getSSRC();
  std::shared_ptr<SequenceNumberTranslator> translator = getTranslatorForSsrc(ssrc, false);


  RtpUtils::forEachRRBlock(packet, [this, translator](RtcpHeader *chead) {
    if (chead->packettype == RTCP_RTP_Feedback_PT) {
      RtpUtils::forEachNack(chead, [this, chead, translator](uint16_t new_seq_num, uint16_t new_plb,
      RtcpHeader* nack_header) {
        uint16_t initial_seq_num = new_seq_num;
        std::vector<uint16_t> seq_nums;
        for (int i = -1; i <= 15; i++) {
          uint16_t seq_num = initial_seq_num + i + 1;
          SequenceNumber input_seq_num = translator->reverse(seq_num);
          if (input_seq_num.type == SequenceNumberType::Valid) {
            seq_nums.push_back(input_seq_num.input);
          } else {
            ELOG_DEBUG("Input is not valid for %d", seq_num);
          }
          ELOG_DEBUG("Lost packet %d, input %d", seq_num, input_seq_num.input);
        }
        if (seq_nums.size() > 0) {
          uint16_t pid = seq_nums[0];
          uint16_t blp = 0;
          for (int index = 1; index < seq_nums.size() ; index++) {
            uint16_t distance = seq_nums[index] - pid - 1;
            blp |= (1 << distance);
          }
          nack_header->setNackPid(pid);
          nack_header->setNackBlp(blp);
          ELOG_DEBUG("Translated pid %u, translated blp %u", pid, blp);
        }
      });
    }
  });
  ctx->fireWrite(packet);
}

bool RtpPaddingRemovalHandler::removePaddingBytes(std::shared_ptr<dataPacket> packet,
    std::shared_ptr<SequenceNumberTranslator> translator) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  int header_length = rtp_header->getHeaderLength();

  int padding_length = RtpUtils::getPaddingLength(packet);
  if (padding_length + header_length == packet->length) {
    uint16_t sequence_number = rtp_header->getSeqNumber();
    translator->get(sequence_number, true);
    return false;
  }
  packet->length -= padding_length;
  rtp_header->padding = 0;
  return true;
}

std::shared_ptr<SequenceNumberTranslator> RtpPaddingRemovalHandler::getTranslatorForSsrc(uint32_t ssrc,
  bool should_create) {
    auto translator_it = translator_map_.find(ssrc);
    std::shared_ptr<SequenceNumberTranslator> translator;
    if (translator_it != translator_map_.end()) {
      translator = translator_it->second;
    } else {
      if (should_create) {
        ELOG_DEBUG("message: no Translator found creating a new one, ssrc: %u", ssrc);
        translator = std::make_shared<SequenceNumberTranslator>();
        translator_map_[ssrc] = translator;
      }
    }
    return translator;
  }

void RtpPaddingRemovalHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  if (initialized_) {
    return;
  }
  initialized_ = true;
}
}  // namespace erizo
