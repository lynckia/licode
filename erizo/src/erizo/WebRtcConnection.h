#ifndef ERIZO_SRC_ERIZO_WEBRTCCONNECTION_H_
#define ERIZO_SRC_ERIZO_WEBRTCCONNECTION_H_

#include <boost/thread/mutex.hpp>

#include <string>
#include <map>
#include <vector>

#include "./logger.h"
#include "./SdpInfo.h"
#include "./MediaDefinitions.h"
#include "./Transport.h"
#include "./Stats.h"
#include "pipeline/Pipeline.h"
#include "thread/Worker.h"
#include "thread/IOWorker.h"
#include "rtp/RtcpProcessor.h"
#include "rtp/RtpExtensionProcessor.h"
#include "lib/Clock.h"
#include "pipeline/Handler.h"
#include "pipeline/Service.h"
#include "rtp/QualityManager.h"
#include "rtp/PacketBufferService.h"

namespace erizo {

constexpr std::chrono::milliseconds kBitrateControlPeriod(100);
constexpr uint32_t kDefaultVideoSinkSSRC = 55543;
constexpr uint32_t kDefaultAudioSinkSSRC = 44444;

class Transport;
class TransportListener;
class IceConfig;
class MediaStream;

/**
 * WebRTC Events
 */
enum WebRTCEvent {
  CONN_INITIAL = 101, CONN_STARTED = 102, CONN_GATHERED = 103, CONN_READY = 104, CONN_FINISHED = 105,
  CONN_CANDIDATE = 201, CONN_SDP = 202,
  CONN_FAILED = 500
};

class WebRtcConnectionEventListener {
 public:
    virtual ~WebRtcConnectionEventListener() {
    }
    virtual void notifyEvent(WebRTCEvent newEvent, const std::string& message) = 0;
};

class WebRtcConnectionStatsListener {
 public:
    virtual ~WebRtcConnectionStatsListener() {
    }
    virtual void notifyStats(const std::string& message) = 0;
};

/**
 * A WebRTC Connection. This class represents a WebRTC Connection that can be established with other peers via a SDP negotiation
 * it comprises all the necessary Transport components.
 */
class WebRtcConnection: public TransportListener, public LogContext,
                        public std::enable_shared_from_this<WebRtcConnection> {
  DECLARE_LOGGER();

 public:
  typedef typename Handler::Context Context;

  /**
   * Constructor.
   * Constructs an empty WebRTCConnection without any configuration.
   */
  WebRtcConnection(std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker,
      const std::string& connection_id, const IceConfig& iceConfig,
      const std::vector<RtpMap> rtp_mappings, const std::vector<erizo::ExtMap> ext_mappings,
      WebRtcConnectionEventListener* listener);
  /**
   * Destructor.
   */
  virtual ~WebRtcConnection();
  /**
   * Inits the WebConnection by starting ICE Candidate Gathering.
   * @return True if the candidates are gathered.
   */
  bool init();
  void close();
  void syncClose();
  /**
   * Sets the SDP of the remote peer.
   * @param sdp The SDP.
   * @return true if the SDP was received correctly.
   */
  bool setRemoteSdp(const std::string &sdp);

  bool createOffer(bool videoEnabled, bool audioEnabled, bool bundle);
  /**
   * Add new remote candidate (from remote peer).
   * @param sdp The candidate in SDP format.
   * @return true if the SDP was received correctly.
   */
  bool addRemoteCandidate(const std::string &mid, int mLineIndex, const std::string &sdp);
  /**
   * Obtains the local SDP.
   * @return The SDP as a string.
   */
  std::string getLocalSdp();

  /**
   * Sends a PLI Packet
   * @return the size of the data sent
   */
  int sendPLI();

  void setQualityLayer(int spatial_layer, int temporal_layer);

  /**
   * Sets the Event Listener for this WebRtcConnection
   */
  inline void setWebRtcConnectionEventListener(WebRtcConnectionEventListener* listener) {
    this->connEventListener_ = listener;
  }

  /**
   * Sets the Stats Listener for this WebRtcConnection
   */
  inline void setWebRtcConnectionStatsListener(
            WebRtcConnectionStatsListener* listener) {
    stats_->setStatsListener(listener);
  }

  /**
   * Gets the current state of the Ice Connection
   * @return
   */
  WebRTCEvent getCurrentState();

  void getJSONStats(std::function<void(std::string)> callback);

  void onTransportData(std::shared_ptr<DataPacket> packet, Transport *transport) override;

  void updateState(TransportState state, Transport * transport) override;

  void onCandidate(const CandidateInfo& cand, Transport *transport) override;

  void setFeedbackReports(bool will_send_feedback, uint32_t target_bitrate = 0);
  void setSlideShowMode(bool state);

  void muteStream(bool mute_video, bool mute_audio);
  void setVideoConstraints(int max_video_width, int max_video_height, int max_video_frame_rate);

  void setMetadata(std::map<std::string, std::string> metadata);

  void read(std::shared_ptr<DataPacket> packet);
  void write(std::shared_ptr<DataPacket> packet);

  void notifyToEventSink(MediaEventPtr event);

  void asyncTask(std::function<void(std::shared_ptr<WebRtcConnection>)> f);

  bool isAudioMuted() { return audio_muted_; }
  bool isVideoMuted() { return video_muted_; }

  std::shared_ptr<MediaStream> getMediaStream() { return media_stream_; }
  void addMediaStream(std::shared_ptr<MediaStream> media_stream);
  void removeMediaStream(const std::string& stream_id);

  std::shared_ptr<Stats> getStatsService() { return stats_; }

  bool isSlideShowModeEnabled() { return slide_show_mode_; }

  RtpExtensionProcessor& getRtpExtensionProcessor() { return extProcessor_; }

  std::shared_ptr<Worker> getWorker() { return worker_; }

  bool isSourceSSRC(uint32_t ssrc);
  bool isSinkSSRC(uint32_t ssrc);

  inline const char* toLog() {
    return ("id: " + connection_id_ + ", " + printLogContext()).c_str();
  }

 private:
  // Utils
  std::string getJSONCandidate(const std::string& mid, const std::string& sdp);
  void trackTransportInfo();

 private:
  std::string connection_id_;
  bool audioEnabled_;
  bool videoEnabled_;
  bool trickleEnabled_;
  bool shouldSendFeedback_;
  bool slide_show_mode_;
  bool sending_;
  int bundle_;
  WebRtcConnectionEventListener* connEventListener_;
  IceConfig iceConfig_;
  std::vector<RtpMap> rtp_mappings_;
  RtpExtensionProcessor extProcessor_;

  uint32_t rateControl_;  // Target bitrate for hacky rate control in BPS

  std::string stunServer_;

  boost::condition_variable cond_;

  time_point now_, mark_;

  std::shared_ptr<Transport> videoTransport_, audioTransport_;

  std::shared_ptr<Stats> stats_;
  WebRTCEvent globalState_;

  boost::mutex updateStateMutex_;  // , slideShowMutex_;

  std::shared_ptr<Worker> worker_;
  std::shared_ptr<IOWorker> io_worker_;
  std::shared_ptr<MediaStream> media_stream_;
  std::shared_ptr<SdpInfo> remote_sdp_;
  std::shared_ptr<SdpInfo> local_sdp_;
  bool audio_muted_;
  bool video_muted_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_WEBRTCCONNECTION_H_
