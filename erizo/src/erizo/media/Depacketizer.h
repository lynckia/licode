#ifndef ERIZO_SRC_ERIZO_MEDIA_DEPACKETIZER_H_
#define ERIZO_SRC_ERIZO_MEDIA_DEPACKETIZER_H_

#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Parser.h"
#include "rtp/RtpH264Parser.h"
#include <memory>
#include <stdexcept>
#include <utility>

namespace erizo {
class Depacketizer_memory_error: public std::runtime_error {
 public:
  template<typename ... Args>
  explicit Depacketizer_memory_error(Args&&... args) :
      std::runtime_error(std::forward<Args>(args)...) {
  }
};

/**
 * Decomposes rtp packets into frames
 */
class Depacketizer {
  static constexpr int buffer_size_ = 1000000;

 public:
  virtual ~Depacketizer() = default;

  unsigned char* frame() {
    return buffer_;
  }

  const unsigned char* frame() const {
    return buffer_;
  }

  int frame_size() const {
    return buffer_ptr_ - buffer_;
  }

  /**
   * Stores and parses a new packet. It doesn't add it the frame just yet.
   * Pkt must remain valid until process is called or a new packet get fetched.
   */
  virtual void fetch_packet(unsigned char* pkt, int len) = 0;

  /**
   * Pushes the last fetched packet into the buffer
   * @returns True if a frame is ready to be grabbed
   */
  virtual bool process_packet() = 0;

  /**
   * Resets the internal state of the depacketizer
   */
  void reset();

  /**
   * @returns True if there's a frame in the buffer and it is a keyframe. The frame may be incomplete.
   */
  virtual bool is_keyframe() const = 0;

 protected:
  void reset_buffer();

  void buffer_check(int len);

  unsigned char* buffer() {
    return buffer_;
  }

  unsigned char* buffer_ptr() {
    return buffer_ptr_;
  }

  void set_buffer_ptr(unsigned char* ptr) {
    buffer_ptr_ = ptr;
  }

 private:
  virtual void reset_impl() = 0;

  unsigned char buffer_[buffer_size_];
  unsigned char* buffer_ptr_ = buffer_;
};

class Vp8_depacketizer: public Depacketizer {
  // Our search state for VP8 frames.
  enum class Search_state {
    lookingForStart, lookingForEnd
  };
 public:
  void fetch_packet(unsigned char* pkt, int len) override;

  bool process_packet() override;

  bool is_keyframe() const override;
 private:
  void reset_impl() override;

  RtpVP8Parser parser_;
  const RtpHeader* head_;
  std::unique_ptr<erizo::RTPPayloadVP8> last_payload_;
  Search_state search_state_ = Search_state::lookingForStart;
};

class H264_depacketizer: public Depacketizer {
  enum class Search_state {
    lookingForStart, lookingForEnd
  };
 public:
  void fetch_packet(unsigned char* pkt, int len) override;

  bool process_packet() override;

  bool is_keyframe() const override;
 private:
  void reset_impl() override;

  RtpH264Parser parser_;
  std::unique_ptr<erizo::RTPPayloadH264> last_payload_;
  Search_state search_state_ = Search_state::lookingForStart;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_MEDIA_DEPACKETIZER_H_
