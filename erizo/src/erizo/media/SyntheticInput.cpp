#include "media/SyntheticInput.h"

#include <algorithm>

#include "lib/Clock.h"
#include "lib/ClockUtils.h"
#include "rtp/RtpHeaders.h"

static constexpr auto kPeriod = std::chrono::milliseconds(20);
static constexpr size_t kMaxPacketSize = 1200;
static constexpr uint8_t kMaxConsecutiveTicks = 20;
static constexpr size_t kVp8PayloadType = 100;
static constexpr size_t kOpusPayloadType = 111;
static constexpr uint32_t kDefaultVideoSsrc = 55543;
static constexpr uint32_t kDefaultAudioSsrc = 44444;
static constexpr auto kAudioPeriod = std::chrono::milliseconds(20);
static constexpr size_t kVideoFramesPerSecond = 15;
static constexpr auto kVideoPeriod = std::chrono::milliseconds(1000 / kVideoFramesPerSecond);
static constexpr auto kDefaultVideoKeyframePeriod = std::chrono::seconds(120);
static constexpr uint32_t kDefaultVideoBitrate = 300000;  // bps
static constexpr uint32_t kDefaultAudioBitrate = 30000;  // bps
static constexpr uint32_t kVideoSampleRate = 90000;  // Hz
static constexpr uint32_t kAudioSampleRate = 48000;  // Hz

namespace erizo {
DEFINE_LOGGER(SyntheticInput, "media.SyntheticInput");
SyntheticInput::SyntheticInput(SyntheticInputConfig config,
                               std::shared_ptr<Worker> worker,
                               std::shared_ptr<Clock> the_clock)
    : clock_{the_clock},
      config_{config},
      worker_{worker},
      video_avg_frame_size_{0},
      video_dev_frame_size_{0},
      video_avg_keyframe_size_{0},
      video_dev_keyframe_size_{0},
      video_period_{kVideoPeriod},
      audio_frame_size_{0},
      audio_period_{kAudioPeriod},
      generator_{random_device_()},
      running_{false},
      video_seq_number_{0},
      audio_seq_number_{0},
      video_ssrc_{kDefaultVideoSsrc},
      audio_ssrc_{kDefaultAudioSsrc},
      total_packets_nacked_{0},
      video_pt_{kVp8PayloadType},
      audio_pt_{kOpusPayloadType},
      next_video_frame_time_{clock_->now() + video_period_},
      next_audio_frame_time_{clock_->now() + audio_period_},
      last_video_keyframe_time_{clock_->now()},
      consecutive_ticks_{0},
      keyframe_requested_{true} {
  calculateSizeAndPeriod(kDefaultVideoBitrate, kDefaultAudioBitrate);
}

SyntheticInput::~SyntheticInput() {
  close();
}

void SyntheticInput::start() {
  if (running_) {
    return;
  }
  running_ = true;
  std::weak_ptr<SyntheticInput> weak_this = shared_from_this();
  worker_->scheduleEvery([weak_this] {
    if (auto this_ptr = weak_this.lock()) {
      if (!this_ptr->running_) {
        return false;
      }
      this_ptr->tick();
      return true;
    }
    return false;
  }, kPeriod);
}

void SyntheticInput::tick() {
  // This method will be called periodically to send audio/video frames
  time_point now = clock_->now();
  if (now >= next_audio_frame_time_) {
    sendAudioFrame(audio_frame_size_);
    next_audio_frame_time_ += audio_period_;
  }
  if (now >= next_video_frame_time_) {
    bool is_keyframe = false;
    size_t frame_size = getRandomValue(video_avg_frame_size_, video_dev_frame_size_);
    if (now - last_video_keyframe_time_ > kDefaultVideoKeyframePeriod || keyframe_requested_) {
      is_keyframe = true;
      frame_size = getRandomValue(video_avg_keyframe_size_, video_dev_keyframe_size_);
    }
    while (frame_size > kMaxPacketSize) {
      sendVideoframe(is_keyframe, false, kMaxPacketSize);
      is_keyframe = false;
      frame_size = frame_size - kMaxPacketSize;
    }
    sendVideoframe(is_keyframe, true, frame_size);

    next_video_frame_time_ += video_period_;
  }
  now = clock_->now();
  if ((next_video_frame_time_ <= now || next_audio_frame_time_ <= now) && consecutive_ticks_ < kMaxConsecutiveTicks) {
    consecutive_ticks_++;
    tick();
  } else {
    consecutive_ticks_ = 0;
  }
}

int SyntheticInput::sendPLI() {
  keyframe_requested_ = true;
  return 0;
}

void SyntheticInput::sendVideoframe(bool is_keyframe, bool is_marker, uint32_t size) {
  erizo::RtpHeader *header = new erizo::RtpHeader();
  header->setSeqNumber(video_seq_number_++);
  header->setTimestamp(ClockUtils::timePointToMs(clock_->now()) * kVideoSampleRate / 1000);
  header->setSSRC(video_ssrc_);
  header->setMarker(is_marker);
  header->setPayloadType(video_pt_);
  char packet_buffer[kMaxPacketSize];
  memset(packet_buffer, 0, size);
  char* data_pointer;
  char* parsing_pointer;
  memcpy(packet_buffer, reinterpret_cast<char*>(header), header->getHeaderLength());
  data_pointer = packet_buffer + header->getHeaderLength();
  parsing_pointer = data_pointer;
  *parsing_pointer = 0x10;
  parsing_pointer++;
  *parsing_pointer = is_keyframe ? 0x00 : 0x01;

  if (is_keyframe) {
    last_video_keyframe_time_ = clock_->now();
    keyframe_requested_ = false;
  }
  if (auto video_sink = video_sink_.lock()) {
    video_sink->deliverVideoData(std::make_shared<DataPacket>(0, packet_buffer, size, VIDEO_PACKET));
  }
  delete header;
}

void SyntheticInput::sendAudioFrame(uint32_t size) {
  erizo::RtpHeader *header = new erizo::RtpHeader();
  header->setSeqNumber(audio_seq_number_++);
  header->setTimestamp(ClockUtils::timePointToMs(clock_->now()) * (kAudioSampleRate / 1000));
  header->setSSRC(audio_ssrc_);
  header->setMarker(true);
  header->setPayloadType(audio_pt_);
  char packet_buffer[kMaxPacketSize];
  memset(packet_buffer, 0, size);
  memcpy(packet_buffer, reinterpret_cast<char*>(header), header->getHeaderLength());
  if (auto audio_sink = audio_sink_.lock()) {
    audio_sink->deliverAudioData(std::make_shared<DataPacket>(0, packet_buffer, size, AUDIO_PACKET));
  }
  delete header;
}

boost::future<void> SyntheticInput::close() {
  running_ = false;
  std::shared_ptr<boost::promise<void>> p = std::make_shared<boost::promise<void>>();
  p->set_value();
  return p->get_future();
}

void SyntheticInput::calculateSizeAndPeriod(uint32_t video_bitrate, uint32_t audio_bitrate) {
  video_bitrate = std::min(video_bitrate, config_.getMaxVideoBitrate());
  video_bitrate = std::max(video_bitrate, config_.getMinVideoBitrate());

  auto video_period = std::chrono::duration_cast<std::chrono::milliseconds>(video_period_);
  auto audio_period = std::chrono::duration_cast<std::chrono::milliseconds>(audio_period_);

  video_avg_frame_size_ = video_period.count() * video_bitrate / 8000;
  video_dev_frame_size_ = video_avg_frame_size_ * 0.1;
  video_avg_keyframe_size_ = video_period.count() * video_bitrate / 8000;
  video_dev_keyframe_size_ = video_avg_keyframe_size_ * 0.01;
  audio_frame_size_ = audio_period.count() * audio_bitrate / 8000;
}

int SyntheticInput::deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(fb_packet->data);
  if (chead->isFeedback()) {
    if (chead->getBlockCount() == 0 && (chead->getLength()+1) * 4  == fb_packet->length) {
      return 0;
    }
    char* moving_buf = fb_packet->data;
    int rtcp_length = 0;
    int total_length = 0;
    do {
      moving_buf += rtcp_length;
      chead = reinterpret_cast<RtcpHeader*>(moving_buf);
      rtcp_length = (ntohs(chead->length) + 1) * 4;
      total_length += rtcp_length;
      switch (chead->packettype) {
        case RTCP_RTP_Feedback_PT:
          // NACKs are already handled by MediaStream. RRs won't be handled.
          total_packets_nacked_++;
          break;
        case RTCP_PS_Feedback_PT:
          switch (chead->getBlockCount()) {
            case RTCP_PLI_FMT:
            case RTCP_FIR_FMT:
              sendPLI();
              break;
            case RTCP_AFB:
              char *unique_id = reinterpret_cast<char*>(&chead->report.rembPacket.uniqueid);
              if (!strncmp(unique_id, "REMB", 4)) {
                uint64_t bitrate = chead->getBrMantis() << chead->getBrExp();
                calculateSizeAndPeriod(bitrate, kDefaultAudioBitrate);
              }
              break;
          }
      }
    } while (total_length < fb_packet->length);
  }
  return 0;
}

uint32_t SyntheticInput::getRandomValue(uint32_t average, uint32_t deviation) {
  std::normal_distribution<> distr(average, deviation);
  return std::round(distr(generator_));
}
}  // namespace erizo
