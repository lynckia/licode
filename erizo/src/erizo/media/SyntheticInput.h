#ifndef ERIZO_SRC_ERIZO_MEDIA_SYNTHETICINPUT_H_
#define ERIZO_SRC_ERIZO_MEDIA_SYNTHETICINPUT_H_

#include <chrono>  // NOLINT
#include <random>

#include "./logger.h"
#include "./MediaDefinitions.h"
#include "thread/Worker.h"
#include "lib/Clock.h"

namespace erizo {

class SyntheticInputConfig {
 public:
  SyntheticInputConfig(uint32_t audio_bitrate, uint32_t min_video_bitrate, uint32_t max_video_bitrate) :
      audio_bitrate_{audio_bitrate}, min_video_bitrate_{min_video_bitrate}, max_video_bitrate_{max_video_bitrate} {}

  uint32_t getMinVideoBitrate() {
    return min_video_bitrate_;
  }

  uint32_t getMaxVideoBitrate() {
    return max_video_bitrate_;
  }

  uint32_t getAudioBitrate() {
    return audio_bitrate_;
  }

 private:
  uint32_t audio_bitrate_;
  uint32_t min_video_bitrate_;
  uint32_t max_video_bitrate_;
};

class SyntheticInput : public MediaSource, public FeedbackSink, public std::enable_shared_from_this<SyntheticInput> {
  DECLARE_LOGGER();

 public:
  explicit SyntheticInput(SyntheticInputConfig config, std::shared_ptr<Worker> worker,
                          std::shared_ptr<Clock> the_clock = std::make_shared<SteadyClock>());
  virtual ~SyntheticInput();
  int sendPLI() override;
  boost::future<void> close() override;
  void start();

 private:
  void tick();
  void calculateSizeAndPeriod(uint32_t video_bitrate, uint32_t audio_bitrate);
  int deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) override;
  void sendVideoframe(bool is_keyframe, bool is_marker, uint32_t size);
  void sendAudioFrame(uint32_t size);
  uint32_t getRandomValue(uint32_t average, uint32_t variation);
  void scheduleEvery(duration period);

 private:
  std::shared_ptr<Clock> clock_;
  SyntheticInputConfig config_;
  std::shared_ptr<Worker> worker_;
  uint32_t video_avg_frame_size_;
  uint32_t video_dev_frame_size_;
  uint32_t video_avg_keyframe_size_;
  uint32_t video_dev_keyframe_size_;
  duration video_period_;
  uint32_t audio_frame_size_;
  duration audio_period_;
  std::random_device random_device_;
  std::mt19937 generator_;
  bool running_;
  uint32_t video_seq_number_;
  uint32_t audio_seq_number_;
  uint32_t video_ssrc_;
  uint32_t audio_ssrc_;
  uint32_t total_packets_nacked_;
  size_t video_pt_;
  size_t audio_pt_;
  time_point next_video_frame_time_;
  time_point next_audio_frame_time_;
  time_point last_video_keyframe_time_;
  uint8_t consecutive_ticks_;
  std::atomic<bool> keyframe_requested_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_SYNTHETICINPUT_H_
