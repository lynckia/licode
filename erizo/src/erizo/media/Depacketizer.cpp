#include "media/Depacketizer.h"
#include <cstring>

namespace erizo {
void Depacketizer::reset_buffer() {
  buffer_ptr_ = buffer_;
}

void Depacketizer::reset() {
  reset_impl();
  reset_buffer();
}

void Depacketizer::buffer_check(const int len) {
  if (len + (buffer_ptr_ - buffer_) >= buffer_size_) {
    reset();
    throw Depacketizer_memory_error("Not enough buffer. Dropping frame. "
        "Please adjust your Depacketizer::buffer_size");
  }
}

void Vp8_depacketizer::fetch_packet(unsigned char* pkt, const int len) {
  head_ = reinterpret_cast<const RtpHeader*>(pkt);
  last_payload_ = std::unique_ptr<erizo::RTPPayloadVP8>(
      parser_.parseVP8(reinterpret_cast<unsigned char*>(pkt + head_->getHeaderLength()),
          len - head_->getHeaderLength()));
}

bool Vp8_depacketizer::process_packet() {
  if (!last_payload_) {
    return false;
  }

  bool endOfFrame = (head_->getMarker() > 0);
  bool startOfFrame = last_payload_->beginningOfPartition;
  bool deliver = false;

  switch (search_state_) {
  case Search_state::lookingForStart:
    if (startOfFrame && endOfFrame) {
      // This packet is a standalone frame.  Send it on.  Look for start.
      reset_buffer();
      buffer_check(last_payload_->dataLength);
      std::memcpy(buffer(), last_payload_->data, last_payload_->dataLength);
      set_buffer_ptr(buffer_ptr() + last_payload_->dataLength);
      deliver = true;
    } else if (!startOfFrame && !endOfFrame) {
      // This is neither the start nor the end of a frame.  Reset our buffers.  Look for start.
      reset_buffer();
    } else if (startOfFrame && !endOfFrame) {
      // Found start frame.  Copy to buffers.  Look for our end.
      buffer_check(last_payload_->dataLength);
      std::memcpy(buffer_ptr(), last_payload_->data, last_payload_->dataLength);
      set_buffer_ptr(buffer_ptr() + last_payload_->dataLength);
      search_state_ = Search_state::lookingForEnd;
    } else {  // (!startOfFrame && endOfFrame)
      // We got the end of a frame.  Reset our buffers.
      reset_buffer();
    }
    break;
  case Search_state::lookingForEnd:
    if (startOfFrame && endOfFrame) {
      // Unexpected.  We were looking for the end of a frame, and got a whole new frame.
      // Reset our buffers, send this frame on, and go to the looking for start state.
      reset_buffer();
      buffer_check(last_payload_->dataLength);
      std::memcpy(buffer_ptr(), last_payload_->data, last_payload_->dataLength);
      set_buffer_ptr(buffer_ptr() + last_payload_->dataLength);
      deliver = true;
      reset_impl();
    } else if (!startOfFrame && !endOfFrame) {
      // This is neither the start nor the end.  Add it to our unpackage buffer.
      buffer_check(last_payload_->dataLength);
      std::memcpy(buffer_ptr(), last_payload_->data, last_payload_->dataLength);
      set_buffer_ptr(buffer_ptr() + last_payload_->dataLength);
    } else if (startOfFrame && !endOfFrame) {
      // Unexpected.  We got the start of a frame.  Clear out our buffer, toss this payload in,
      // and continue looking for the end.
      reset_buffer();
      buffer_check(last_payload_->dataLength);
      std::memcpy(buffer_ptr(), last_payload_->data, last_payload_->dataLength);
      set_buffer_ptr(buffer_ptr() + last_payload_->dataLength);
    } else {  // (!startOfFrame && endOfFrame)
      // Got the end of a frame.  Let's deliver and start looking for the start of a frame.
      search_state_ = Search_state::lookingForStart;
      buffer_check(last_payload_->dataLength);
      std::memcpy(buffer_ptr(), last_payload_->data, last_payload_->dataLength);
      set_buffer_ptr(buffer_ptr() + last_payload_->dataLength);
      deliver = true;
    }
    break;
  }

  return deliver;
}

bool Vp8_depacketizer::is_keyframe() const {
  if (!last_payload_) {
    return false;
  }
  return last_payload_->frameType == VP8FrameTypes::kVP8IFrame;
}

void Vp8_depacketizer::reset_impl() {
  last_payload_ = nullptr;
  search_state_ = Search_state::lookingForStart;
}

void H264_depacketizer::fetch_packet(unsigned char* pkt, int len) {
  const RtpHeader* head = reinterpret_cast<const RtpHeader*>(pkt);
  last_payload_ = std::unique_ptr<erizo::RTPPayloadH264>(
      parser_.parseH264(reinterpret_cast<unsigned char*>(pkt + head->getHeaderLength()),
          len - head->getHeaderLength()));
}

bool H264_depacketizer::is_keyframe() const {
  if (!last_payload_) {
    return false;
  }
  return last_payload_->frameType == H264FrameTypes::kH264IFrame;
}

void H264_depacketizer::reset_impl() {
  last_payload_ = nullptr;
  search_state_ = Search_state::lookingForStart;
}

bool H264_depacketizer::process_packet() {
  switch (last_payload_->nal_type) {
  case single: {
    if (search_state_ == Search_state::lookingForEnd) {
      reset();
    }
    if (last_payload_->dataLength == 0) {
      return false;
    }
    const auto total_size = last_payload_->dataLength + sizeof(RTPPayloadH264::start_sequence);
    buffer_check(total_size);
    std::memcpy(buffer_ptr(), RTPPayloadH264::start_sequence, sizeof(RTPPayloadH264::start_sequence));
    set_buffer_ptr(buffer_ptr() + sizeof(RTPPayloadH264::start_sequence));
    std::memcpy(buffer_ptr(), last_payload_->data, last_payload_->dataLength);
    set_buffer_ptr(buffer_ptr() + last_payload_->dataLength);
    return true;
  }
  case fragmented: {
    if (last_payload_->dataLength == 0) {
      return false;
    }
    const auto total_size = last_payload_->dataLength
        + sizeof(RTPPayloadH264::start_sequence)
        + last_payload_->fragment_nal_header_len;
    buffer_check(total_size);
    if (last_payload_->start_bit) {
      if (search_state_ == Search_state::lookingForEnd) {
        reset();
      }
      search_state_ = Search_state::lookingForEnd;
      std::memcpy(buffer_ptr(), RTPPayloadH264::start_sequence, sizeof(RTPPayloadH264::start_sequence));
      set_buffer_ptr(buffer_ptr() + sizeof(RTPPayloadH264::start_sequence));
      std::memcpy(buffer_ptr(), &last_payload_->fragment_nal_header, last_payload_->fragment_nal_header_len);
      set_buffer_ptr(buffer_ptr() + last_payload_->fragment_nal_header_len);
    }
    std::memcpy(buffer_ptr(), last_payload_->data, last_payload_->dataLength);
    set_buffer_ptr(buffer_ptr() + last_payload_->dataLength);
    if (last_payload_->end_bit) {
      search_state_ = Search_state::lookingForStart;
      return true;
    }
    break;
  }
  case aggregated: {
    if (last_payload_->unpacked_data_len == 0) {
      return false;
    }
    buffer_check(last_payload_->unpacked_data_len);
    std::memcpy(buffer_ptr(), &last_payload_->unpacked_data[0], last_payload_->unpacked_data_len);
    set_buffer_ptr(buffer_ptr() + last_payload_->unpacked_data_len);
    return true;
  }
  }
  return false;
}
}  // namespace erizo
