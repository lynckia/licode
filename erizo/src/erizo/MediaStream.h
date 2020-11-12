
#ifndef ERIZO_SRC_ERIZO_MEDIASTREAM_H_
#define ERIZO_SRC_ERIZO_MEDIASTREAM_H_

#include <boost/thread/mutex.hpp>
#include <boost/thread/future.hpp>

#include <atomic>
#include <string>
#include <map>
#include <random>
#include <vector>

#include "./logger.h"
#include "./SdpInfo.h"
#include "./MediaDefinitions.h"
#include "./Stats.h"
#include "./Transport.h"
#include "./WebRtcConnection.h"
#include "pipeline/Pipeline.h"
#include "thread/Worker.h"
#include "rtp/RtcpProcessor.h"
#include "rtp/RtpExtensionProcessor.h"
#include "lib/Clock.h"
#include "pipeline/Handler.h"
#include "pipeline/HandlerManager.h"
#include "pipeline/Service.h"
#include "rtp/QualityManager.h"
#include "rtp/PacketBufferService.h"

namespace erizo {

class MediaStreamStatsListener {
 public:
    virtual ~MediaStreamStatsListener() {
    }
    virtual void notifyStats(const std::string& message) = 0;
};


class MediaStreamEventListener {
 public:
    virtual ~MediaStreamEventListener() {
    }
    virtual void notifyMediaStreamEvent(const std::string& type, const std::string& message) = 0;
};

class PublisherInfo {
 public:
  PublisherInfo() : audio_fraction_lost{0}, video_fraction_lost{0} {}
  PublisherInfo(uint8_t audio_fl, uint8_t video_fl)
    : audio_fraction_lost{audio_fl}, video_fraction_lost{video_fl} {}
  uint8_t audio_fraction_lost;
  uint8_t video_fraction_lost;
};

/**
 * A MediaStream. This class represents a Media Stream that can be established with other peers via a SDP negotiation
 */
class MediaStream: public MediaSink, public MediaSource, public FeedbackSink,
                        public FeedbackSource, public LogContext, public HandlerManagerListener,
                        public std::enable_shared_from_this<MediaStream>, public Service {
  DECLARE_LOGGER();
  static log4cxx::LoggerPtr statsLogger;

 public:
  typedef typename Handler::Context Context;
  bool audio_enabled_;
  bool video_enabled_;

  /**
   * Constructor.
   * Constructs an empty MediaStream without any configuration.
   */
  MediaStream(std::shared_ptr<Worker> worker, std::shared_ptr<WebRtcConnection> connection,
      const std::string& media_stream_id, const std::string& media_stream_label,
      bool is_publisher, int session_version);
  /**
   * Destructor.
   */
  virtual ~MediaStream();
  void init();
  bool configure(bool doNotWaitForRemoteSdp);
  boost::future<void> close() override;
  virtual uint32_t getMaxVideoBW();
  virtual uint32_t getBitrateFromMaxQualityLayer() { return bitrate_from_max_quality_layer_; }
  virtual uint32_t getVideoBitrate() { return video_bitrate_; }
  void setVideoBitrate(uint32_t bitrate) { video_bitrate_ = bitrate; }
  void setMaxVideoBW(uint32_t max_video_bw);
  void syncClose();
  bool setRemoteSdp(std::shared_ptr<SdpInfo> sdp, int session_version_negotiated);

  /**
   * Sends a PLI Packet
   * @return the size of the data sent
   */
  int sendPLI() override;
  void sendPLIToFeedback();
  void setQualityLayer(int spatial_layer, int temporal_layer);
  void enableSlideShowBelowSpatialLayer(bool enabled, int spatial_layer);
  void setPeriodicKeyframeRequests(bool activate, uint32_t interval_in_ms = 0);

  WebRTCEvent getCurrentState();

  /**
   * Sets the Event Listener for this MediaStream
   */
  void setMediaStreamEventListener(MediaStreamEventListener* listener);

  void notifyMediaStreamEvent(const std::string& type, const std::string& message);

  /**
   * Sets the Stats Listener for this MediaStream
   */
  inline void setMediaStreamStatsListener(
            MediaStreamStatsListener* listener) {
    stats_->setStatsListener(listener);
  }

  void getJSONStats(std::function<void(std::string)> callback);

  virtual void onTransportData(std::shared_ptr<DataPacket> packet, Transport *transport);

  void sendPacketAsync(std::shared_ptr<DataPacket> packet);

  void setTransportInfo(std::string audio_info, std::string video_info);

  void setFeedbackReports(bool will_send_feedback, uint32_t target_bitrate = 0);
  void setSlideShowMode(bool state);
  void muteStream(bool mute_video, bool mute_audio);
  void setVideoConstraints(int max_video_width, int max_video_height, int max_video_frame_rate);

  void setMetadata(std::map<std::string, std::string> metadata);

  void read(std::shared_ptr<DataPacket> packet);
  void write(std::shared_ptr<DataPacket> packet);

  void enableHandler(const std::string &name);
  void disableHandler(const std::string &name);
  void notifyUpdateToHandlers() override;

  void notifyToEventSink(MediaEventPtr event);


  boost::future<void> asyncTask(std::function<void(std::shared_ptr<MediaStream>)> f);

  void initializeStats();
  void printStats();

  bool isAudioMuted() { return audio_muted_; }
  bool isVideoMuted() { return video_muted_; }

  std::shared_ptr<SdpInfo> getRemoteSdpInfo() { return remote_sdp_; }

  virtual bool isSlideShowModeEnabled() { return slide_show_mode_; }

  virtual bool isRequestingPeriodicKeyframes() { return periodic_keyframes_requested_; }
  virtual uint32_t getPeriodicKeyframesRequesInterval() { return periodic_keyframe_interval_; }

  virtual bool isSimulcast() { return simulcast_; }
  void setSimulcast(bool simulcast) { simulcast_ = simulcast; }

  RtpExtensionProcessor& getRtpExtensionProcessor() { return connection_->getRtpExtensionProcessor(); }
  std::shared_ptr<Worker> getWorker() { return worker_; }

  std::string getId() { return stream_id_; }
  std::string getLabel() { return mslabel_; }

  bool isSourceSSRC(uint32_t ssrc);
  bool isSinkSSRC(uint32_t ssrc);
  void parseIncomingPayloadType(char *buf, int len, packetType type);
  void parseIncomingExtensionId(char *buf, int len, packetType type);
  virtual void setTargetPaddingBitrate(uint64_t bitrate);
  virtual uint64_t getTargetPaddingBitrate() { return target_padding_bitrate_; }

  virtual uint32_t getTargetVideoBitrate();

  bool isPipelineInitialized() { return pipeline_initialized_; }
  bool isRunning() { return pipeline_initialized_ && sending_; }
  bool isReady() { return ready_; }
  Pipeline::Ptr getPipeline() { return pipeline_; }
  bool isPublisher() { return is_publisher_; }
  void setBitrateFromMaxQualityLayer(uint64_t bitrate) { bitrate_from_max_quality_layer_ = bitrate; }

  inline std::string toLog() {
    return "id: " + stream_id_ + ", role:" + (is_publisher_ ? "publisher" : "subscriber") + ", " + printLogContext();
  }

  virtual PublisherInfo getPublisherInfo() { return publisher_info_; }

 private:
  void sendPacket(std::shared_ptr<DataPacket> packet);
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override;
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override;
  int deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) override;
  int deliverEvent_(MediaEventPtr event) override;
  void initializePipeline();
  void transferLayerStats(std::string spatial, std::string temporal);
  void transferMediaStats(std::string target_node, std::string source_parent, std::string source_node);

  void changeDeliverExtensionId(DataPacket *dp, packetType type);
  void changeDeliverPayloadType(DataPacket *dp, packetType type);
  // parses incoming payload type, replaces occurence in buf
  uint32_t getRandomValue(uint32_t min, uint32_t max);

 private:
  boost::mutex event_listener_mutex_;
  MediaStreamEventListener* media_stream_event_listener_;
  std::shared_ptr<WebRtcConnection> connection_;
  std::string stream_id_;
  std::string mslabel_;
  bool should_send_feedback_;
  bool slide_show_mode_;
  bool sending_;
  bool ready_;
  int bundle_;

  uint32_t rate_control_;  // Target bitrate for hacky rate control in BPS

  std::string stun_server_;

  time_point now_, mark_;

  std::shared_ptr<RtcpProcessor> rtcp_processor_;
  std::shared_ptr<Stats> stats_;
  std::shared_ptr<Stats> log_stats_;
  std::shared_ptr<QualityManager> quality_manager_;
  std::shared_ptr<PacketBufferService> packet_buffer_;
  std::shared_ptr<HandlerManager> handler_manager_;

  Pipeline::Ptr pipeline_;

  std::shared_ptr<Worker> worker_;

  bool audio_muted_;
  bool video_muted_;

  bool pipeline_initialized_;

  bool is_publisher_;

  std::atomic_bool simulcast_;
  std::atomic<uint64_t> bitrate_from_max_quality_layer_;
  std::atomic<uint32_t> video_bitrate_;
  std::random_device random_device_;
  std::mt19937 random_generator_;
  uint64_t target_padding_bitrate_;
  bool periodic_keyframes_requested_;
  uint32_t periodic_keyframe_interval_;
  int session_version_;
  PublisherInfo publisher_info_;

 protected:
  std::shared_ptr<SdpInfo> remote_sdp_;
};

class PacketReader : public InboundHandler {
 public:
  explicit PacketReader(MediaStream *media_stream) : media_stream_{media_stream} {}

  void enable() override {}
  void disable() override {}

  std::string getName() override {
    return "reader";
  }

  void read(Context *ctx, std::shared_ptr<DataPacket> packet) override {
    media_stream_->read(std::move(packet));
  }

  void notifyUpdate() override {
  }

 private:
  MediaStream *media_stream_;
};

class PacketWriter : public OutboundHandler {
 public:
  explicit PacketWriter(MediaStream *media_stream) : media_stream_{media_stream} {}

  void enable() override {}
  void disable() override {}

  std::string getName() override {
    return "writer";
  }

  void write(Context *ctx, std::shared_ptr<DataPacket> packet) override {
    media_stream_->write(std::move(packet));
  }

  void notifyUpdate() override {
  }

 private:
  MediaStream *media_stream_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIASTREAM_H_
