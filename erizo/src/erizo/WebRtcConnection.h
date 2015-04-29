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

class SrData {
  public:
    uint32_t srNtp;
    struct timeval timestamp;

    SrData() {
      srNtp = 0;
      timestamp = (struct timeval){0} ;
    };
    SrData(uint32_t srNTP, struct timeval theTimestamp){
      this->srNtp = srNTP;
      this->timestamp = theTimestamp;
    }
};


class RtcpData {
  // lost packets - list and length
  public:
    uint32_t *nackList;
    int nackLen;

    // current values - tracks packet lost for fraction calculation
    uint32_t packetCount;
    uint16_t rrsReceivedInPeriod;

    uint32_t ssrc;
    uint32_t totalPacketsLost;
    uint32_t ratioLost:8;
    uint32_t highestSeqNumReceived;
    uint32_t lastSr;
    uint64_t reportedBandwidth;
    uint32_t delaySinceLastSr;
    
    uint32_t lastDelay;

    uint32_t jitter;
    // last SR field
    uint32_t lastSrTimestamp;
    // required to properly calculate DLSR
    uint16_t nackSeqnum;
    uint16_t nackBlp;

    // time based data flow limits
    struct timeval lastUpdated, lastSent, lastSrUpdated;
    struct timeval lastREMBSent, lastPliSent;
    struct timeval lastSrReception;
    // to prevent sending too many reports, track time of last
    struct timeval lastRrSent;
    
    bool shouldSendPli;
    bool shouldSendREMB;
    bool shouldSendNACK;
    // flag to send receiver report
    bool requestRr;
    bool hasSentFirstRr;

    std::list<boost::shared_ptr<SrData>> senderReports;

    void reset(){
      ratioLost = 0;
//      lastSrTimestamp = 0;
      lastSr = 0;
//      delaySinceLastSr = 0;
      requestRr = false;
      jitter = 0;
      rrsReceivedInPeriod = 0;
//      highestSeqNumReceived = 0;
      reportedBandwidth = 0;
      lastDelay = 0;
    }

    RtcpData(){
      packetCount = 0;        
      rrsReceivedInPeriod = 0;
      totalPacketsLost = 0;
      ratioLost = 0;
      highestSeqNumReceived = 0;
      lastSr = 0;
      reportedBandwidth = 0;
      delaySinceLastSr = 0;
      jitter = 0;
      lastSrTimestamp = 0;
      requestRr = false;
      hasSentFirstRr = false;
      lastDelay = 0;
     
      shouldSendPli = false;
      shouldSendREMB = false;
      shouldSendNACK = false;
      nackSeqnum = 0;
      nackBlp = 0;
      lastRrSent = (struct timeval){0};
      lastPliSent = (struct timeval){0};
      lastREMBSent = (struct timeval){0};
      lastSrReception = (struct timeval){0};
    }

    // lock for any blocking data change
    boost::mutex dataLock;
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
    WebRtcConnection(bool audioEnabled, bool videoEnabled, const std::string &stunServer, int stunPort, int minPort, int maxPort,bool trickleEnabled,WebRtcConnectionEventListener* listener);
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
    int addREMB(char* buf, int len, uint32_t bitrate);
    int addNACK(char* buf, int len, uint16_t seqNum, uint16_t blp, uint32_t sourceSsrc);
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

    void checkRtcpFb();

    void sendReceiverReport();

    void setFeedbackReports(bool shouldSendFb){
      this->shouldSendFeedback_ = shouldSendFb;
    };


    // webrtc::RtpHeader overrides.
    int32_t OnReceivedPayloadData(const uint8_t* payloadData, const uint16_t payloadSize,const webrtc::WebRtcRTPHeader* rtpHeader);
    bool OnRecoveredPacket(const uint8_t* packet, int packet_length);

private:
  static const int STATS_INTERVAL = 5000;
  static const int RTCP_PERIOD = 150;
  static const int PLI_THRESHOLD = 50;
  static const uint64_t NTPTOMSCONV = 4294967296;
  
  SdpInfo remoteSdp_;
  SdpInfo localSdp_;

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
  void analyzeFeedback(char* buf, int len);
  std::string getJSONCandidate(const std::string& mid, const std::string& sdp);
  std::map<uint32_t, boost::shared_ptr<RtcpData>> rtcpData_;

  
  bool audioEnabled_;
  bool videoEnabled_;
  bool trickleEnabled_;
  bool shouldSendFeedback_;

  int stunPort_, minPort_, maxPort_;
  std::string stunServer_;
  

	boost::condition_variable cond_;
  webrtc::FecReceiverImpl fec_receiver_;
};

} /* namespace erizo */
#endif /* WEBRTCCONNECTION_H_ */
