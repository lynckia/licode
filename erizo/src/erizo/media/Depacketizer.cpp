#include "media/Depacketizer.h"
#include <cstring>

namespace erizo {
DEFINE_LOGGER(Depacketizer, "media.Depacketizer");

void Depacketizer::resetBuffer() {
  buffer_ptr_ = buffer_;
}

void Depacketizer::reset() {
  resetImpl();
  resetBuffer();
}

bool Depacketizer::bufferCheck(const int len) {
  if (len + (buffer_ptr_ - buffer_) >= kUnpackageBufferSize) {
    reset();
    ELOG_ERROR("Not enough buffer. Dropping frame. Please adjust your kUnpackageBufferSize in Depacketizer.h");
    return false;
  }
  return true;
}

void Vp8Depacketizer::fetchPacket(unsigned char* pkt, const int len) {
  head_ = reinterpret_cast<const RtpHeader*>(pkt);
  last_payload_ = std::unique_ptr<erizo::RTPPayloadVP8>(
      parser_.parseVP8(reinterpret_cast<unsigned char*>(pkt + head_->getHeaderLength()),
          len - head_->getHeaderLength()));
}

bool Vp8Depacketizer::processPacket() {
  if (!last_payload_) {
    return false;
  }

  bool endOfFrame = (head_->getMarker() > 0);
  bool startOfFrame = last_payload_->beginningOfPartition;
  bool deliver = false;

  switch (search_state_) {
  case SearchState::lookingForStart:
    if (startOfFrame && endOfFrame) {
      // This packet is a standalone frame.  Send it on.  Look for start.
      resetBuffer();
      if (bufferCheck(last_payload_->dataLength)) {
        std::memcpy(buffer(), last_payload_->data, last_payload_->dataLength);
        setBufferPtr(getBufferPtr() + last_payload_->dataLength);
        deliver = true;
      }
    } else if (!startOfFrame && !endOfFrame) {
      // This is neither the start nor the end of a frame.  Reset our buffers.  Look for start.
      resetBuffer();
    } else if (startOfFrame && !endOfFrame) {
      // Found start frame.  Copy to buffers.  Look for our end.
      if (bufferCheck(last_payload_->dataLength)) {
        std::memcpy(getBufferPtr(), last_payload_->data, last_payload_->dataLength);
        setBufferPtr(getBufferPtr() + last_payload_->dataLength);
        search_state_ = SearchState::lookingForEnd;
      }
    } else {  // (!startOfFrame && endOfFrame)
      // We got the end of a frame.  Reset our buffers.
      resetBuffer();
    }
    break;
  case SearchState::lookingForEnd:
    if (startOfFrame && endOfFrame) {
      // Unexpected.  We were looking for the end of a frame, and got a whole new frame.
      // Reset our buffers, send this frame on, and go to the looking for start state.
      resetBuffer();
      if (bufferCheck(last_payload_->dataLength)) {
        std::memcpy(getBufferPtr(), last_payload_->data, last_payload_->dataLength);
        setBufferPtr(getBufferPtr() + last_payload_->dataLength);
        deliver = true;
        resetImpl();
      }
    } else if (!startOfFrame && !endOfFrame) {
      // This is neither the start nor the end.  Add it to our unpackage buffer.
      if (bufferCheck(last_payload_->dataLength)) {
        std::memcpy(getBufferPtr(), last_payload_->data, last_payload_->dataLength);
        setBufferPtr(getBufferPtr() + last_payload_->dataLength);
      }
    } else if (startOfFrame && !endOfFrame) {
      // Unexpected.  We got the start of a frame.  Clear out our buffer, toss this payload in,
      // and continue looking for the end.
      resetBuffer();
      if (bufferCheck(last_payload_->dataLength)) {
        std::memcpy(getBufferPtr(), last_payload_->data, last_payload_->dataLength);
        setBufferPtr(getBufferPtr() + last_payload_->dataLength);
      }
    } else {  // (!startOfFrame && endOfFrame)
      // Got the end of a frame.  Let's deliver and start looking for the start of a frame.
      search_state_ = SearchState::lookingForStart;
      if (bufferCheck(last_payload_->dataLength)) {
        std::memcpy(getBufferPtr(), last_payload_->data, last_payload_->dataLength);
        setBufferPtr(getBufferPtr() + last_payload_->dataLength);
        deliver = true;
      }
    }
    break;
  }

  return deliver;
}

bool Vp8Depacketizer::isKeyframe() const {
  if (!last_payload_) {
    return false;
  }
  return last_payload_->frameType == VP8FrameTypes::kVP8IFrame;
}

void Vp8Depacketizer::resetImpl() {
  last_payload_ = nullptr;
  search_state_ = SearchState::lookingForStart;
}

void H264Depacketizer::fetchPacket(unsigned char* pkt, int len) {
  const RtpHeader* head = reinterpret_cast<const RtpHeader*>(pkt);
  last_payload_ = std::unique_ptr<erizo::RTPPayloadH264>(
      parser_.parseH264(reinterpret_cast<unsigned char*>(pkt + head->getHeaderLength()),
          len - head->getHeaderLength()));
}

bool H264Depacketizer::isKeyframe() const {
  if (!last_payload_) {
    return false;
  }
  return last_payload_->frameType == H264FrameTypes::kH264IFrame;
}

void H264Depacketizer::resetImpl() {
  last_payload_ = nullptr;
  search_state_ = SearchState::lookingForStart;
}

bool H264Depacketizer::processPacket() {
  switch (last_payload_->nal_type) {
    case single: {
      if (search_state_ == SearchState::lookingForEnd) {
        reset();
      }
      if (last_payload_->dataLength == 0) {
        return false;
      }
      const auto total_size = last_payload_->dataLength + sizeof(RTPPayloadH264::start_sequence);
      if (bufferCheck(total_size)) {
        std::memcpy(getBufferPtr(), RTPPayloadH264::start_sequence, sizeof(RTPPayloadH264::start_sequence));
        setBufferPtr(getBufferPtr() + sizeof(RTPPayloadH264::start_sequence));
        std::memcpy(getBufferPtr(), last_payload_->data, last_payload_->dataLength);
        setBufferPtr(getBufferPtr() + last_payload_->dataLength);
        return true;
      }
      break;
    }
    case fragmented: {
      if (last_payload_->dataLength == 0) {
        return false;
      }
      const auto total_size = last_payload_->dataLength
          + sizeof(RTPPayloadH264::start_sequence)
          + last_payload_->fragment_nal_header_len;
      if (bufferCheck(total_size)) {
        if (last_payload_->start_bit) {
          if (search_state_ == SearchState::lookingForEnd) {
            reset();
          }
          search_state_ = SearchState::lookingForEnd;
          std::memcpy(getBufferPtr(), RTPPayloadH264::start_sequence, sizeof(RTPPayloadH264::start_sequence));
          setBufferPtr(getBufferPtr() + sizeof(RTPPayloadH264::start_sequence));
          std::memcpy(getBufferPtr(), &last_payload_->fragment_nal_header, last_payload_->fragment_nal_header_len);
          setBufferPtr(getBufferPtr() + last_payload_->fragment_nal_header_len);
        }
        std::memcpy(getBufferPtr(), last_payload_->data, last_payload_->dataLength);
        setBufferPtr(getBufferPtr() + last_payload_->dataLength);
        if (last_payload_->end_bit) {
          search_state_ = SearchState::lookingForStart;
          return true;
        }
      }
      break;
    }
    case aggregated: {
      if (last_payload_->unpacked_data_len == 0) {
        return false;
      }
      if (bufferCheck(last_payload_->unpacked_data_len)) {
        std::memcpy(getBufferPtr(), &last_payload_->unpacked_data[0], last_payload_->unpacked_data_len);
        setBufferPtr(getBufferPtr() + last_payload_->unpacked_data_len);
        return true;
      }
    }
  }
  return false;
}
}  // namespace erizo
