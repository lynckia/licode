#ifndef ERIZO_SRC_ERIZO_MEDIA_EXTERNALOUTPUT_H_
#define ERIZO_SRC_ERIZO_MEDIA_EXTERNALOUTPUT_H_

#include <boost/thread.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <string>

#include "./MediaDefinitions.h"
#include "thread/Worker.h"
#include "rtp/RtpPacketQueue.h"
#include "rtp/RtpExtensionProcessor.h"
#include "webrtc/modules/rtp_rtcp/source/ulpfec_receiver_impl.h"
#include "media/MediaProcessor.h"
#include "media/Depacketizer.h"
#include "./Stats.h"
#include "lib/Clock.h"
#include "SdpInfo.h"
#include "rtp/QualityManager.h"
#include "pipeline/Handler.h"
#include "pipeline/HandlerManager.h"

#include "./logger.h"

namespace erizo {

static constexpr uint64_t kExternalOutputMaxBitrate = 1000000000;

class ExternalOutput : public MediaSink, public RawDataReceiver, public FeedbackSource,
                       public webrtc::RtpData, public HandlerManagerListener,
                       public std::enable_shared_from_this<ExternalOutput> {
  DECLARE_LOGGER();

 public:
  explicit ExternalOutput(std::shared_ptr<Worker> worker, const std::string& output_url,
                          const std::vector<RtpMap> rtp_mappings,
                          const std::vector<erizo::ExtMap> ext_mappings);
  virtual ~ExternalOutput();
  bool init();
  void receiveRawData(const RawDataPacket& packet) override;

  // webrtc::RtpData callbacks.  This is for Forward Error Correction (per rfc5109) handling.
  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override;
  int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                        size_t payload_size,
                                        const webrtc::WebRtcRTPHeader* rtp_header) override;

  boost::future<void> close() override;

  void write(std::shared_ptr<DataPacket> packet);

  void notifyUpdateToHandlers() override;

  bool isRecording() { return recording_; }

 private:
  std::shared_ptr<Worker> worker_;
  Pipeline::Ptr pipeline_;
  std::unique_ptr<webrtc::UlpfecReceiver> fec_receiver_;
  RtpPacketQueue audio_queue_, video_queue_;
  std::atomic<bool> recording_, inited_;
  boost::mutex mtx_;  // a mutex we use to signal our writer thread that data is waiting.
  boost::thread thread_;
  boost::condition_variable cond_;
  AVStream *video_stream_, *audio_stream_;
  AVFormatContext *context_;

  uint32_t video_source_ssrc_;
  std::unique_ptr<Depacketizer> depacketizer_;

  // Timestamping strategy: we use the RTP timestamps so we don't have to restamp and we're not
  // subject to error due to the RTP packet queue depth and playout.
  //
  // However, the units of our audio and video RTP timestamps are not comparable because:
  //
  // 1. RTP timestamps start at a random value
  // 2. The units are completely different.  Video is typically on a 90khz clock, whereas audio
  //    typically follows whatever the sample rate is (e.g. 8khz for PCMU, 48khz for Opus, etc.)
  //
  // So, our strategy is to keep track of the first audio and video timestamps we encounter so we
  // can adjust subsequent timestamps by that much so our timestamps roughly start at zero.
  //
  // Audio and video can also start at different times, and it's possible video wouldn't even arrive
  // at all (or arrive late) so we also need to keep track of a start time offset.  We also need to track
  // this *before* stuff enters the RTP packet queue, since that guy will mess with the timing of stuff that's
  // outputted in an attempt to re-order incoming packets.  So when we receive an audio or video packet,
  // we set first_data_received_.  We then use that to compute audio/videoStartTimeOffset_ appropriately,
  // and that value is added to every timestamp we write.
  long long first_video_timestamp_;  // NOLINT
  long long first_audio_timestamp_;  // NOLINT
  clock::time_point first_data_received_;  // NOLINT
  long long video_offset_ms_;  // NOLINT
  long long audio_offset_ms_;  // NOLINT


  // The last sequence numbers we received for audio and video.  Allows us to react to packet loss.
  uint16_t last_video_sequence_number_;
  uint16_t last_audio_sequence_number_;

  // our VP8 frame search state.  We're always looking for either the beginning or the end of a frame.
  // Note: VP8 purportedly has two packetization schemes; per-frame and per-partition.  A frame is
  // composed of one or more partitions.  However, we don't seem to be sent anything but partition 0
  // so the second scheme seems not applicable.  Too bad.
  bool need_to_send_fir_;
  std::vector<RtpMap> rtp_mappings_;
  enum AVCodecID video_codec_;
  enum AVCodecID audio_codec_;
  std::map<uint, RtpMap> video_maps_;
  std::map<uint, RtpMap> audio_maps_;
  RtpMap video_map_;
  RtpMap audio_map_;
  bool pipeline_initialized_;
  std::shared_ptr<Stats> stats_;
  std::shared_ptr<QualityManager> quality_manager_;
  std::shared_ptr<HandlerManager> handler_manager_;
  RtpExtensionProcessor ext_processor_;

  bool initContext();
  int sendFirPacket();
  boost::future<void> asyncTask(std::function<void(std::shared_ptr<ExternalOutput>)> f);
  void queueData(char* buffer, int length, packetType type);
  void queueDataAsync(std::shared_ptr<DataPacket> copied_packet);
  void sendLoop();
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override;
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override;
  int deliverEvent_(MediaEventPtr event) override;
  void writeAudioData(char* buf, int len);
  void writeVideoData(char* buf, int len);
  void updateVideoCodec(RtpMap map);
  void updateAudioCodec(RtpMap map);
  void maybeWriteVideoPacket(char* buf, int len);
  void initializePipeline();
  void syncClose();
  AVDictionary* genVideoMetadata();
};

class ExternalOuputWriter : public OutboundHandler {
 public:
  explicit ExternalOuputWriter(std::shared_ptr<ExternalOutput> output) : output_{output} {}

  void enable() override {}
  void disable() override {}

  std::string getName() override {
    return "writer";
  }

  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override {
    if (auto output = output_.lock()) {
      output->write(std::move(packet));
    }
  }

  void notifyUpdate() override {
  }

 private:
  std::weak_ptr<ExternalOutput> output_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_EXTERNALOUTPUT_H_
