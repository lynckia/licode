#ifndef ERIZO_SRC_ERIZO_MEDIA_EXTERNALOUTPUT_H_
#define ERIZO_SRC_ERIZO_MEDIA_EXTERNALOUTPUT_H_

#include <boost/thread.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <string>

#include "./MediaDefinitions.h"
#include "rtp/RtpPacketQueue.h"
#include "webrtc/modules/rtp_rtcp/source/ulpfec_receiver_impl.h"
#include "media/MediaProcessor.h"
#include "lib/Clock.h"

#include "./logger.h"

namespace erizo {

#define UNPACKAGE_BUFFER_SIZE 200000

class WebRtcConnection;

// Our search state for VP8 frames.
enum vp8SearchState {
    lookingForStart,
    lookingForEnd
};

class ExternalOutput : public MediaSink, public RawDataReceiver, public FeedbackSource, public webrtc::RtpData {
  DECLARE_LOGGER();

 public:
  explicit ExternalOutput(const std::string& outputUrl);
  virtual ~ExternalOutput();
  bool init();
  void receiveRawData(const RawDataPacket& packet) override;

  // webrtc::RtpData callbacks.  This is for Forward Error Correction (per rfc5109) handling.
  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override;
  int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                        size_t payload_size,
                                        const webrtc::WebRtcRTPHeader* rtp_header) override;

  void close() override;

 private:
  std::unique_ptr<webrtc::UlpfecReceiver> fec_receiver_;
  RtpPacketQueue audioQueue_, videoQueue_;
  bool recording_, inited_;
  boost::mutex mtx_;  // a mutex we use to signal our writer thread that data is waiting.
  boost::thread thread_;
  boost::condition_variable cond_;
  AVStream *video_stream_, *audio_stream_;
  AVFormatContext *context_;

  int unpackagedSize_;
  uint32_t videoSourceSsrc_;
  unsigned char* unpackagedBufferpart_;
  unsigned char unpackagedBuffer_[UNPACKAGE_BUFFER_SIZE];

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
  uint16_t lastVideoSequenceNumber_;
  uint16_t lastAudioSequenceNumber_;

  // our VP8 frame search state.  We're always looking for either the beginning or the end of a frame.
  // Note: VP8 purportedly has two packetization schemes; per-frame and per-partition.  A frame is
  // composed of one or more partitions.  However, we don't seem to be sent anything but partition 0
  // so the second scheme seems not applicable.  Too bad.
  vp8SearchState vp8SearchState_;
  bool needToSendFir_;

  bool initContext();
  int sendFirPacket();
  void queueData(char* buffer, int length, packetType type);
  void sendLoop();
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override;
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override;
  int deliverEvent_(MediaEventPtr event) override;
  void writeAudioData(char* buf, int len);
  void writeVideoData(char* buf, int len);
  bool bufferCheck(RTPPayloadVP8* payload);
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_EXTERNALOUTPUT_H_
