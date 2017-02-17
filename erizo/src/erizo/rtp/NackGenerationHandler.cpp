#include <algorithm>
#include "rtp/NackGenerationHandler.h"
#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"

namespace erizo {

DEFINE_LOGGER(NackGenerationHandler, "rtp.NackGenerationHandler");

const int kMaxRetransmits = 2;
const int kMaxNacks = 253;

NackGenerationHandler::NackGenerationHandler(bool use_timing)
  : connection_{nullptr}, enabled_{true}, initialized_{false} {}

void NackGenerationHandler::enable() {
  enabled_ = true;
}

void NackGenerationHandler::disable() {
  enabled_ = false;
}


void NackGenerationHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  if (packet->type == VIDEO_PACKET) {
    handleRtpPacket(packet);
  }
  ctx->fireRead(packet);
}

void NackGenerationHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  ctx->fireWrite(packet);
}

void NackGenerationHandler::notifyUpdate() {
  if (initialized_) {
    return;
  }

  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  connection_ = pipeline->getService<WebRtcConnection>().get();
  if (!connection_) {
    return;
  }
  std::vector<uint32_t> video_ssrc_list = connection_->getVideoSourceSSRCList();
  std::for_each(video_ssrc_list.begin(), video_ssrc_list.end(), [this] (uint32_t video_ssrc){
      ELOG_DEBUG("message: Add ssrc, ssrc: %u", video_ssrc);
  });
}

bool NackGenerationHandler::rtpSequenceLessThan(uint16_t x, uint16_t y) {
  int diff = y - x;
  if (diff > 0) {
    return (diff < 0x8000);
  } else if (diff < 0) {
    return (diff < -0x8000);
  } else {
    return false;
  }
}

void NackGenerationHandler::handleRtpPacket(std::shared_ptr<dataPacket> packet) {
  RtpHeader *head = reinterpret_cast<RtpHeader*>(packet->data);
  uint16_t seq_num = head->getSeqNumber();
  uint32_t ssrc = head->getSSRC();
  auto highest_pos = highest_seq_num_map_.find(ssrc);
  if (highest_pos == highest_seq_num_map_.end()) {
    highest_seq_num_map_[ssrc] = seq_num;
    return;
  }
  uint16_t highest_seq_num = highest_pos.second();
  if (seq_num == highest_seq_num)
    return 0;
  ELOG_DEBUG("message: Found Highest seq number, ssrc: %u, highest_seq_num: %u", ssrc, highest_seq_num);
  if (rtpSequenceLessThan(seq_num, highest_seq_num)) {
    ELOG_DEBUG("message: packet out of order, ssrc: %u, seq_num: %u, highest_seq_num: %u",
        seq_num, highest_seq_num, ssrc);
    //  Look for it in nack list, remove it if its there
  }
  //  if packet is newer than currentMaxPacket+1 -- Send nack for the gap
  addNacks(ssrc, seq_num, highest_seq_num);
  highest_pos.second = seq_num;
  sendNacks(ssrc);
}

void NackGenerationHandler::addNacks(uint32_t ssrc, uint16_t seq_num, uint16_t highest_seq_num) {
  auto nack_list = nack_info_ssrc_map_[ssrc];
  for (uint32_t current_seq_num = highest_seq_num, current_seq_num <= seq_num, current_seq_num++) {
    auto found_nack = std::find_if(nack_list->begin(), nack_list->end(), [current_seq_num](&NackInfo nack) {
        return nack.seq_num == current_seq_num;
        });
    if (found_nack != nack_list->end()) {
      if (found_nack->retransmits >= kMaxRetransmits) {
        nack_list->erase(found_nack);
      }
      found_nack->retransmits++;
    } else {
      ELOG_DEBUG("message: Inserting a new Nack in list, ssrc: %u, seq_num: %u", ssrc, seq_num);
      nack_info.push_back(NackInfo(current_seq_num, 0));
    }
  }
}

void NackGenerationHandler::sendNacks(uint32_t ssrc) {
  // Goes through the list adds blocks of 16 in compound packets (adds more PID/BLP blocks) max is 10 blocks
}

}  // namespace erizo
