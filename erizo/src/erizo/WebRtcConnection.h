#ifndef WEBRTCCONNECTION_H_
#define WEBRTCCONNECTION_H_

#include <string>
#include <queue>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include "logger.h"
#include "SdpInfo.h"
#include "MediaDefinitions.h"
#include "Transport.h"
#include "Stats.h"
#include "rtp/webrtc/fec_receiver_impl.h"
#include "rtp/RtcpProcessor.h"

namespace erizo {

class Transport;
class TransportListener;

/**
 * WebRTC Events
 */
enum WebRTCEvent {
  CONN_INITIAL = 101, CONN_STARTED = 102,CONN_GATHERED = 103, CONN_READY = 104, CONN_FINISHED = 105, CONN_CANDIDATE = 201, CONN_SDP = 202,
  CONN_FAILED = 500
};

class WebRtcConnectionEventListener {
public:
    virtual ~WebRtcConnectionEventListener() {
    }
    ;
    virtual void notifyEvent(WebRTCEvent newEvent, const std::string& message)=0;

};

class WebRtcConnectionStatsListener {
public:
    virtual ~WebRtcConnectionStatsListener() {
    }
    ;
    virtual void notifyStats(const std::string& message)=0;
};





/**
 * A WebRTC Connection. This class represents a WebRTC Connection that can be established with other peers via a SDP negotiation
 * it comprises all the necessary Transport components.
 */
class WebRtcConnection: public MediaSink, public MediaSource, public FeedbackSink, public FeedbackSource, public TransportListener, public webrtc::RtpData {
	DECLARE_LOGGER();
public:
    /**
     * Constructor.
     * Constructs an empty WebRTCConnection without any configuration.
     */
    WebRtcConnection(bool audioEnabled, bool videoEnabled, const std::string &stunServer, int stunPort, int minPort, int maxPort,bool trickleEnabled, WebRtcConnectionEventListener* listener);
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
    /**
     * Sets the SDP of the remote peer.
     * @param sdp The SDP.
     * @return true if the SDP was received correctly.
     */
    bool setRemoteSdp(const std::string &sdp);

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
  
    // changes the outgoing payload type for in the given data packet
    void changeDeliverPayloadType(dataPacket *dp, packetType type);
    // parses incoming payload type, replaces occurence in buf
    void parseIncomingPayloadType(char *buf, int len, packetType type);

    /**
     * Sends a PLI Packet 
     * @return the size of the data sent
     */
    int sendPLI();  
  /**
   * Sets the Event Listener for this WebRtcConnection
   */

    inline void setWebRtcConnectionEventListener(
            WebRtcConnectionEventListener* listener){
    this->connEventListener_ = listener;
  }
    
  /**
   * Sets the Stats Listener for this WebRtcConnection
   */
  inline void setWebRtcConnectionStatsListener(
            WebRtcConnectionStatsListener* listener){
    this->thisStats_.setStatsListener(listener);
  }
    /**
     * Gets the current state of the Ice Connection
     * @return
     */
    WebRTCEvent getCurrentState();
    
    std::string getJSONStats();

    void onTransportData(char* buf, int len, Transport *transport);

    void updateState(TransportState state, Transport * transport);

    void queueData(int comp, const char* data, int len, Transport *transport, packetType type);

    void onCandidate(const CandidateInfo& cand, Transport *transport);

    void setFeedbackReports(bool shouldSendFb, uint32_t rateControl=0){
      this->shouldSendFeedback_ = shouldSendFb;
      this->rateControl_ = rateControl;
    };


    // webrtc::RtpHeader overrides.
    int32_t OnReceivedPayloadData(const uint8_t* payloadData, const uint16_t payloadSize,const webrtc::WebRtcRTPHeader* rtpHeader);
    bool OnRecoveredPacket(const uint8_t* packet, int packet_length);

private:
  static const int STATS_INTERVAL = 5000;
  
  SdpInfo remoteSdp_;
  SdpInfo localSdp_;

  boost::shared_ptr<RtcpProcessor> rtcpProcessor_;

  Stats thisStats_;

	WebRTCEvent globalState_;

  int bundle_, sequenceNumberFIR_;
  boost::mutex receiveVideoMutex_, updateStateMutex_, feedbackMutex_;
  boost::thread send_Thread_;
	std::queue<dataPacket> sendQueue_;
	WebRtcConnectionEventListener* connEventListener_;
	Transport *videoTransport_, *audioTransport_;

  bool sending_;
	void sendLoop();
	void writeSsrc(char* buf, int len, unsigned int ssrc);
	int deliverAudioData_(char* buf, int len);
	int deliverVideoData_(char* buf, int len);
  int deliverFeedback_(char* buf, int len);
  std::string getJSONCandidate(const std::string& mid, const std::string& sdp);

  
  bool audioEnabled_;
  bool videoEnabled_;
  bool trickleEnabled_;
  bool shouldSendFeedback_;
  uint32_t rateControl_; //Target bitrate for hacky rate control in BPS 

  int stunPort_, minPort_, maxPort_;
  std::string stunServer_;

  webrtc::FecReceiverImpl fec_receiver_;
	boost::condition_variable cond_;


  struct timeval now_, mark_;
};

} /* namespace erizo */
#endif /* WEBRTCCONNECTION_H_ */
