#include "rtp/RtpPaddingRemovalHandler.h"
#include "rtp/RtpUtils.h"
#include "MediaStream.h"

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

void RtpPaddingRemovalHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
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
      ELOG_DEBUG("Invalid translation %u, ssrc: %u", sequence_number, ssrc);
      return;
    }
    ELOG_DEBUG("Changing seq_number from %u to %u, ssrc %u", sequence_number, sequence_number_info.output,
     ssrc);
    rtp_header->setSeqNumber(sequence_number_info.output);
  }
  ctx->fireRead(std::move(packet));
}

void RtpPaddingRemovalHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
  RtcpHeader* rtcp_head = reinterpret_cast<RtcpHeader*>(packet->data);
  if (!enabled_ || packet->type != VIDEO_PACKET || !rtcp_head->isFeedback()) {
    ctx->fireWrite(std::move(packet));
    return;
  }
  uint32_t ssrc = rtcp_head->getSourceSSRC();
  std::shared_ptr<SequenceNumberTranslator> translator = getTranslatorForSsrc(ssrc, false);
  if (!translator) {
    ELOG_DEBUG("No translator for ssrc %u, %s", ssrc, stream_->toLog());
    ctx->fireWrite(std::move(packet));
    return;
  }
  RtpUtils::forEachRtcpBlock(packet, [this, translator, ssrc](RtcpHeader *chead) {
    if (chead->packettype == RTCP_RTP_Feedback_PT) {
      RtpUtils::forEachNack(chead, [this, translator, ssrc](uint16_t new_seq_num, uint16_t new_plb,
      RtcpHeader* nack_header) {
        uint16_t initial_seq_num = new_seq_num;
        std::vector<uint16_t> seq_nums;
        for (int i = -1; i <= 15; i++) {
          uint16_t seq_num = initial_seq_num + i + 1;
          SequenceNumber input_seq_num = translator->reverse(seq_num);
          if (input_seq_num.type == SequenceNumberType::Valid) {
            seq_nums.push_back(input_seq_num.input);
          } else {
            ELOG_DEBUG("Input is not valid for %u, ssrc %u, %s", seq_num, ssrc, stream_->toLog());
          }
          ELOG_DEBUG("Lost packet %u, input %u, ssrc %u", seq_num, input_seq_num.input, ssrc);
        }
        if (seq_nums.size() > 0) {
          uint16_t pid = seq_nums[0];
          uint16_t blp = 0;
          for (uint16_t index = 1; index < seq_nums.size() ; index++) {
            uint16_t distance = seq_nums[index] - pid - 1;
            blp |= (1 << distance);
          }
          nack_header->setNackPid(pid);
          nack_header->setNackBlp(blp);
          ELOG_DEBUG("Translated pid %u, translated blp %u, ssrc %u, %s", pid, blp, ssrc, stream_->toLog());
        }
      });
    }
  });
  ctx->fireWrite(std::move(packet));
}

bool RtpPaddingRemovalHandler::removePaddingBytes(std::shared_ptr<DataPacket> packet,
    std::shared_ptr<SequenceNumberTranslator> translator) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  int header_length = rtp_header->getHeaderLength();

  int padding_length = RtpUtils::getPaddingLength(packet);
  if (padding_length + header_length == packet->length) {
    uint16_t sequence_number = rtp_header->getSeqNumber();
    translator->get(sequence_number, true);
    ELOG_DEBUG("Dropping packet %u, %s", sequence_number, stream_->toLog());
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
      ELOG_DEBUG("Found Translator for %u, %s", ssrc, stream_->toLog());
      translator = translator_it->second;
    } else if (should_create) {
      ELOG_DEBUG("message: no Translator found creating a new one, ssrc: %u, %s", ssrc,
      stream_->toLog());
      translator = std::make_shared<SequenceNumberTranslator>();
      translator_map_[ssrc] = translator;
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
  stream_ = pipeline->getService<MediaStream>().get();
  initialized_ = true;
}
}  // namespace erizo
