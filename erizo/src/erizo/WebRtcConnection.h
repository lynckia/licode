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
#include "webrtc/modules/rtp_rtcp/include/ulpfec_receiver.h"
#include "rtp/RtcpProcessor.h"
#include "rtp/RtpExtensionProcessor.h"
#include "rtp/RtpSlideShowHandler.h"
#include "rtp/RtpVP8SlideShowHandler.h"
#include "rtp/RtpAudioMuteHandler.h"
#include "lib/Clock.h"

namespace erizo {

constexpr std::chrono::milliseconds kBitrateControlPeriod(100);

class Transport;
class TransportListener;
class IceConfig;

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
class WebRtcConnection: public MediaSink, public MediaSource, public FeedbackSink,
                        public FeedbackSource, public TransportListener, public webrtc::RtpData, public LogContext,
                        public std::enable_shared_from_this<WebRtcConnection> {
  DECLARE_LOGGER();

 public:
  typedef typename Handler::Context Context;

  /**
   * Constructor.
   * Constructs an empty WebRTCConnection without any configuration.
   */
  WebRtcConnection(std::shared_ptr<Worker> worker, const std::string& connection_id, const IceConfig& iceConfig,
      const std::vector<RtpMap> rtp_mappings, WebRtcConnectionEventListener* listener);
  /**
   * Destructor.
   */
  virtual ~WebRtcConnection();
  /**
   * Inits the WebConnection by starting ICE Candidate Gathering.
   * @return True if the candidates are gathered.
   */
  bool init();
  void close() override;
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

  int deliverAudioData(char* buf, int len);
  int deliverVideoData(char* buf, int len);
  int deliverFeedback(char* buf, int len);

  /**
   * Sends a PLI Packet
   * @return the size of the data sent
   */
  int sendPLI() override;
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
    this->thisStats_.setStatsListener(listener);
  }

  /**
   * Gets the current state of the Ice Connection
   * @return
   */
  WebRTCEvent getCurrentState();

  std::string getJSONStats();

  void onTransportData(std::shared_ptr<dataPacket> packet, Transport *transport) override;

  void updateState(TransportState state, Transport * transport) override;

  void sendPacketAsync(std::shared_ptr<dataPacket> packet);

  void onCandidate(const CandidateInfo& cand, Transport *transport) override;

  void setFeedbackReports(bool will_send_feedback, uint32_t target_bitrate = 0);
  void setSlideShowMode(bool state);
  void muteStream(bool mute_video, bool mute_audio);

  void setMetadata(std::map<std::string, std::string> metadata);

  // webrtc::RtpHeader overrides.
  int32_t OnReceivedPayloadData(const uint8_t* payloadData, size_t payloadSize,
                                const webrtc::WebRtcRTPHeader* rtpHeader) override;
  bool OnRecoveredPacket(const uint8_t* packet, size_t packet_length) override;

  void read(std::shared_ptr<dataPacket> packet);
  void write(std::shared_ptr<dataPacket> packet);

  inline const char* toLog() {
    return ("id: " + connection_id_ + ", " + printLogContext()).c_str();
  }

 private:
  std::string connection_id_;
  SdpInfo remoteSdp_;
  SdpInfo localSdp_;
  bool audioEnabled_;
  bool videoEnabled_;
  bool trickleEnabled_;
  bool shouldSendFeedback_;
  bool slideShowMode_;
  bool sending_;
  int bundle_;
  WebRtcConnectionEventListener* connEventListener_;
  IceConfig iceConfig_;
  std::vector<RtpMap> rtp_mappings_;
  RtpExtensionProcessor extProcessor_;

  uint32_t rateControl_;  // Target bitrate for hacky rate control in BPS

  int stunPort_, minPort_, maxPort_;
  std::string stunServer_;

  std::unique_ptr<webrtc::UlpfecReceiver> fec_receiver_;
  boost::condition_variable cond_;

  time_point now_, mark_;

  boost::shared_ptr<RtcpProcessor> rtcpProcessor_;
  std::shared_ptr<Transport> videoTransport_, audioTransport_;

  Stats thisStats_;
  WebRTCEvent globalState_;

  boost::mutex updateStateMutex_;  // , slideShowMutex_;

  Pipeline::Ptr pipeline_;

  std::shared_ptr<Worker> worker_;

  std::shared_ptr<RtpSlideShowHandler> slideshow_handler_;
  std::shared_ptr<RtpAudioMuteHandler> audio_mute_handler_;

  void sendPacket(std::shared_ptr<dataPacket> packet);
  int deliverAudioData_(char* buf, int len) override;
  int deliverVideoData_(char* buf, int len) override;
  int deliverFeedback_(char* buf, int len) override;


  // Utils
  std::string getJSONCandidate(const std::string& mid, const std::string& sdp);
  // changes the outgoing payload type for in the given data packet
  void changeDeliverPayloadType(dataPacket *dp, packetType type);
  // parses incoming payload type, replaces occurence in buf
  void parseIncomingPayloadType(char *buf, int len, packetType type);
};

class PacketReader : public InboundHandler {
 public:
  explicit PacketReader(WebRtcConnection *connection) : connection_{connection} {}

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override {
    connection_->read(packet);
  }

 private:
  WebRtcConnection *connection_;
};

class PacketWriter : public OutboundHandler {
 public:
  explicit PacketWriter(WebRtcConnection *connection) : connection_{connection} {}

  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override {
    connection_->write(packet);
  }

 private:
  WebRtcConnection *connection_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_WEBRTCCONNECTION_H_
