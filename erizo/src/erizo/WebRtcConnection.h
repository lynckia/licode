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
#include "bandwidth/BandwidthDistributionAlgorithm.h"
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
  CONN_CANDIDATE = 201, CONN_SDP = 202, CONN_SDP_PROCESSED = 203,
  CONN_FAILED = 500
};

class WebRtcConnectionEventListener {
 public:
    virtual ~WebRtcConnectionEventListener() {
    }
    virtual void notifyEvent(WebRTCEvent newEvent, const std::string& message, const std::string &stream_id = "") = 0;
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
      const std::string& connection_id, const IceConfig& ice_config,
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

  bool setRemoteSdpInfo(std::shared_ptr<SdpInfo> sdp, std::string stream_id);
  /**
   * Sets the SDP of the remote peer.
   * @param sdp The SDP.
   * @return true if the SDP was received correctly.
   */
  bool setRemoteSdp(const std::string &sdp, std::string stream_id);

  bool createOffer(bool video_enabled, bool audio_enabled, bool bundle);
  /**
   * Add new remote candidate (from remote peer).
   * @param sdp The candidate in SDP format.
   * @return true if the SDP was received correctly.
   */
  bool addRemoteCandidate(const std::string &mid, int mLineIndex, const std::string &sdp);
  /**
   * Obtains the local SDP.
   * @return The SDP as a SdpInfo.
   */
  std::shared_ptr<SdpInfo> getLocalSdpInfo();
  /**
   * Obtains the local SDP.
   * @return The SDP as a string.
   */
  std::string getLocalSdp();

  /**
   * Sets the Event Listener for this WebRtcConnection
   */
  void setWebRtcConnectionEventListener(WebRtcConnectionEventListener* listener);

  /**
   * Gets the current state of the Ice Connection
   * @return
   */
  WebRTCEvent getCurrentState();

  void onTransportData(std::shared_ptr<DataPacket> packet, Transport *transport) override;

  void updateState(TransportState state, Transport * transport) override;

  void onCandidate(const CandidateInfo& cand, Transport *transport) override;

  void setMetadata(std::map<std::string, std::string> metadata);

  void write(std::shared_ptr<DataPacket> packet);
  void syncWrite(std::shared_ptr<DataPacket> packet);

  void asyncTask(std::function<void(std::shared_ptr<WebRtcConnection>)> f);

  bool isAudioMuted() { return audio_muted_; }
  bool isVideoMuted() { return video_muted_; }

  void addMediaStream(std::shared_ptr<MediaStream> media_stream);
  void removeMediaStream(const std::string& stream_id);
  void forEachMediaStream(std::function<void(const std::shared_ptr<MediaStream>&)> func);
  void forEachMediaStreamAsync(std::function<void(const std::shared_ptr<MediaStream>&)> func);

  void setTransport(std::shared_ptr<Transport> transport);  // Only for Testing purposes

  std::shared_ptr<Stats> getStatsService() { return stats_; }

  RtpExtensionProcessor& getRtpExtensionProcessor() { return extension_processor_; }

  std::shared_ptr<Worker> getWorker() { return worker_; }

  inline std::string toLog() {
    return "id: " + connection_id_ + ", " + printLogContext();
  }

 private:
  bool processRemoteSdp(std::string stream_id);
  void setRemoteSdpsToMediaStreams(std::string stream_id);
  void onRemoteSdpsSetToMediaStreams(std::string stream_id);
  std::string getJSONCandidate(const std::string& mid, const std::string& sdp);
  void trackTransportInfo();
  void onRtcpFromTransport(std::shared_ptr<DataPacket> packet, Transport *transport);
  void onREMBFromTransport(RtcpHeader *chead, Transport *transport);
  void maybeNotifyWebRtcConnectionEvent(const WebRTCEvent& event, const std::string& message,
        const std::string& stream_id = "");

 protected:
  std::atomic<WebRTCEvent> global_state_;

 private:
  std::string connection_id_;
  bool audio_enabled_;
  bool video_enabled_;
  bool trickle_enabled_;
  bool slide_show_mode_;
  bool sending_;
  int bundle_;
  WebRtcConnectionEventListener* conn_event_listener_;
  IceConfig ice_config_;
  std::vector<RtpMap> rtp_mappings_;
  RtpExtensionProcessor extension_processor_;
  boost::condition_variable cond_;

  std::shared_ptr<Transport> video_transport_, audio_transport_;

  std::shared_ptr<Stats> stats_;

  boost::mutex update_state_mutex_;
  boost::mutex event_listener_mutex_;

  std::shared_ptr<Worker> worker_;
  std::shared_ptr<IOWorker> io_worker_;
  std::vector<std::shared_ptr<MediaStream>> media_streams_;
  std::shared_ptr<SdpInfo> remote_sdp_;
  std::shared_ptr<SdpInfo> local_sdp_;
  bool audio_muted_;
  bool video_muted_;
  bool first_remote_sdp_processed_;

  std::unique_ptr<BandwidthDistributionAlgorithm> distributor_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_WEBRTCCONNECTION_H_
