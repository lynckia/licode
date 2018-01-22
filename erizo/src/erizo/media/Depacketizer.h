#ifndef ERIZO_SRC_ERIZO_MEDIA_DEPACKETIZER_H_
#define ERIZO_SRC_ERIZO_MEDIA_DEPACKETIZER_H_

#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtpH264Parser.h"
#include "./logger.h"
#include <memory>
#include <stdexcept>
#include <utility>

namespace erizo {

/**
 * Decomposes rtp packets into frames
 */
class Depacketizer {
  DECLARE_LOGGER();
  static constexpr int kUnpackageBufferSize = 1000000;

 public:
  virtual ~Depacketizer() = default;

  unsigned char* frame() {
    return buffer_;
  }

  const unsigned char* frame() const {
    return buffer_;
  }

  int frameSize() const {
    return buffer_ptr_ - buffer_;
  }

  /**
   * Stores and parses a new packet. It doesn't add it the frame just yet.
   * Pkt must remain valid until process is called or a new packet get fetched.
   */
  virtual void fetchPacket(unsigned char* pkt, int len) = 0;

  /**
   * Pushes the last fetched packet into the buffer
   * @returns True if a frame is ready to be grabbed
   */
  virtual bool processPacket() = 0;

  /**
   * Resets the internal state of the depacketizer
   */
  void reset();

  /**
   * @returns True if there's a frame in the buffer and it is a keyframe. The frame may be incomplete.
   */
  virtual bool isKeyframe() const = 0;

 protected:
  enum class SearchState {
    lookingForStart, lookingForEnd
  };

  void resetBuffer();

  bool bufferCheck(int len);

  unsigned char* buffer() {
    return buffer_;
  }

  unsigned char* getBufferPtr() {
    return buffer_ptr_;
  }

  void setBufferPtr(unsigned char* ptr) {
    buffer_ptr_ = ptr;
  }

  SearchState search_state_ = SearchState::lookingForStart;

 private:
  virtual void resetImpl() = 0;

  unsigned char buffer_[kUnpackageBufferSize];
  unsigned char* buffer_ptr_ = buffer_;
};

class Vp8Depacketizer: public Depacketizer {
  // Our search state for VP8 frames.
 public:
  void fetchPacket(unsigned char* pkt, int len) override;

  bool processPacket() override;

  bool isKeyframe() const override;
 private:
  void resetImpl() override;

  RtpVP8Parser parser_;
  const RtpHeader* head_;
  std::unique_ptr<erizo::RTPPayloadVP8> last_payload_;
};

class H264Depacketizer: public Depacketizer {
 public:
  void fetchPacket(unsigned char* pkt, int len) override;

  bool processPacket() override;

  bool isKeyframe() const override;
 private:
  void resetImpl() override;

  RtpH264Parser parser_;
  std::unique_ptr<erizo::RTPPayloadH264> last_payload_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_MEDIA_DEPACKETIZER_H_
