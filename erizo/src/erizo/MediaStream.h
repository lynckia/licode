
#ifndef ERIZO_SRC_ERIZO_MEDIASTREAM_H_
#define ERIZO_SRC_ERIZO_MEDIASTREAM_H_

#include <boost/thread/mutex.hpp>

#include <string>
#include <map>
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

/**
 * A MediaStream. This class represents a Media Stream that can be established with other peers via a SDP negotiation
 */
class MediaStream: public MediaSink, public MediaSource, public FeedbackSink,
                        public FeedbackSource, public LogContext,
                        public std::enable_shared_from_this<MediaStream>, public Service {
  DECLARE_LOGGER();

 public:
  typedef typename Handler::Context Context;
  bool audio_enabled_;
  bool video_enabled_;

  /**
   * Constructor.
   * Constructs an empty MediaStream without any configuration.
   */
  MediaStream(std::shared_ptr<WebRtcConnection> connection,
      const std::string& media_stream_id);
  /**
   * Destructor.
   */
  virtual ~MediaStream();
  bool init();
  void close() override;
  bool setRemoteSdp(std::shared_ptr<SdpInfo> sdp);
  bool setLocalSdp(std::shared_ptr<SdpInfo> sdp);

  /**
   * Sends a PLI Packet
   * @return the size of the data sent
   */
  int sendPLI() override;
  void sendPLIToFeedback();
  void setQualityLayer(int spatial_layer, int temporal_layer);

  WebRTCEvent getCurrentState();

  /**
   * Sets the Stats Listener for this MediaStream
   */
  inline void setMediaStreamStatsListener(
            MediaStreamStatsListener* listener) {
    stats_->setStatsListener(listener);
  }

  void getJSONStats(std::function<void(std::string)> callback);

  void onTransportData(std::shared_ptr<DataPacket> packet, Transport *transport);

  void sendPacketAsync(std::shared_ptr<DataPacket> packet);


  void setFeedbackReports(bool will_send_feedback, uint32_t target_bitrate = 0);
  void setSlideShowMode(bool state);
  void muteStream(bool mute_video, bool mute_audio);
  void setVideoConstraints(int max_video_width, int max_video_height, int max_video_frame_rate);

  void setMetadata(std::map<std::string, std::string> metadata);

  void read(std::shared_ptr<DataPacket> packet);
  void write(std::shared_ptr<DataPacket> packet);

  void enableHandler(const std::string &name);
  void disableHandler(const std::string &name);
  void notifyUpdateToHandlers();

  void notifyToEventSink(MediaEventPtr event);

  void asyncTask(std::function<void(std::shared_ptr<MediaStream>)> f);

  bool isAudioMuted() { return audio_muted_; }
  bool isVideoMuted() { return video_muted_; }

  SdpInfo* getRemoteSdpInfo() { return remote_sdp_.get(); }

  bool isSlideShowModeEnabled() { return slide_show_mode_; }

  RtpExtensionProcessor& getRtpExtensionProcessor() { return connection_->getRtpExtensionProcessor(); }
  std::shared_ptr<Worker> getWorker() { return worker_; }

  std::string& getId() { return stream_id_; }

  bool isSourceSSRC(uint32_t ssrc);
  bool isSinkSSRC(uint32_t ssrc);
  void parseIncomingPayloadType(char *buf, int len, packetType type);

  bool isPipelineInitialized() { return pipeline_initialized_; }
  Pipeline::Ptr getPipeline() { return pipeline_; }

  inline std::string toLog() {
    return "id: " + stream_id_ + ", " + printLogContext();
  }

 private:
  void sendPacket(std::shared_ptr<DataPacket> packet);
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override;
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override;
  int deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) override;
  int deliverEvent_(MediaEventPtr event) override;
  void initializePipeline();

  void changeDeliverPayloadType(DataPacket *dp, packetType type);
  // parses incoming payload type, replaces occurence in buf

 private:
  std::shared_ptr<WebRtcConnection> connection_;
  std::string stream_id_;
  bool should_send_feedback_;
  bool slide_show_mode_;
  bool sending_;
  int bundle_;

  uint32_t rate_control_;  // Target bitrate for hacky rate control in BPS

  std::string stun_server_;

  time_point now_, mark_;

  std::shared_ptr<RtcpProcessor> rtcp_processor_;
  std::shared_ptr<Stats> stats_;
  std::shared_ptr<QualityManager> quality_manager_;
  std::shared_ptr<PacketBufferService> packet_buffer_;

  Pipeline::Ptr pipeline_;

  std::shared_ptr<Worker> worker_;

  bool audio_muted_;
  bool video_muted_;

  bool pipeline_initialized_;
 protected:
  std::shared_ptr<SdpInfo> remote_sdp_;
  std::shared_ptr<SdpInfo> local_sdp_;
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
