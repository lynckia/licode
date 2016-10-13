/*
 * NiceConnection.h
 */

#ifndef ERIZO_SRC_ERIZO_NICECONNECTION_H_
#define ERIZO_SRC_ERIZO_NICECONNECTION_H_

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <string>
#include <vector>
#include <queue>
#include <map>

#include "./MediaDefinitions.h"
#include "./SdpInfo.h"
#include "./logger.h"

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainContext GMainContext;
typedef struct _GMainLoop GMainLoop;

typedef unsigned int uint;

namespace erizo {

#define NICE_STREAM_MAX_UFRAG   256 + 1  /* ufrag + NULL */
#define NICE_STREAM_MAX_UNAME   256 * 2 + 1 + 1 /* 2*ufrag + colon + NULL */
#define NICE_STREAM_MAX_PWD     256 + 1  /* pwd + NULL */
#define NICE_STREAM_DEF_UFRAG   4 + 1    /* ufrag + NULL */
#define NICE_STREAM_DEF_PWD     22 + 1   /* pwd + NULL */

// forward declarations
typedef boost::shared_ptr<dataPacket> packetPtr;
class CandidateInfo;
class WebRtcConnection;

struct CandidatePair{
  std::string erizoCandidateIp;
  int erizoCandidatePort;
  std::string clientCandidateIp;
  int clientCandidatePort;
};

class IceConfig {
 public:
    std::string turnServer, turnUsername, turnPass;
    std::string stunServer;
    uint16_t stunPort, turnPort, minPort, maxPort;
    bool shouldTrickle;
    IceConfig(){
      turnServer = "";
      turnUsername = "";
      turnPass = "";
      stunServer = "";
      stunPort = 0;
      turnPort = 0;
      minPort = 0;
      maxPort = 0;
      shouldTrickle = false;
    }
};

/**
 * States of ICE
 */
enum IceState {
  NICE_INITIAL, NICE_CANDIDATES_RECEIVED, NICE_READY, NICE_FINISHED, NICE_FAILED
};

class NiceConnectionListener {
 public:
    virtual void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* conn) = 0;
    virtual void onCandidate(const CandidateInfo &candidate, NiceConnection *conn) = 0;
    virtual void updateIceState(IceState state, NiceConnection *conn) = 0;
};

/**
 * An ICE connection via libNice
 * Represents an ICE Connection in an new thread.
 *
 */
class NiceConnection {
  DECLARE_LOGGER();

 public:
  /**
   * The MediaType of the connection
   */
  MediaType mediaType;
  /**
   * The transport name
   */
  boost::scoped_ptr<std::string> transportName;
  /**
   * The Obtained local candidates.
   */
  boost::shared_ptr<std::vector<CandidateInfo> > localCandidates;

  /**
   * Constructs a new NiceConnection.
   * @param med The MediaType of the connection.
   * @param transportName The name of the transport protocol. Was used when WebRTC used video_rtp instead of just rtp.
   * @param iceComponents Number of ice components pero connection. Default is 1 (rtcp-mux).
   */
  NiceConnection(MediaType med, const std::string &transportName, const std::string& connection_id,
                 NiceConnectionListener* listener, unsigned int iceComponents,
                 const IceConfig& iceConfig, std::string username = "", std::string password = "");

  virtual ~NiceConnection();
  /**
   * Starts Gathering candidates in a new thread.
   */
  void start();
  /**
   * Sets the remote ICE Candidates.
   * @param candidates A vector containing the CandidateInfo.
   * @return true if successfull.
   */
  bool setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool isBundle);
  /**
   * Sets the local ICE Candidates. Called by C Nice functions.
   * @param candidates A vector containing the CandidateInfo.
   * @return true if successfull.
   */
  void gatheringDone(uint stream_id);
  /**
   * Sets a local ICE Candidates. Called by C Nice functions.
   * @param candidate info to look for
   */
  void getCandidate(uint stream_id, uint component_id, const std::string &foundation);
  /**
   * Get local ICE credentials.
   * @return username and password where credentials will be stored
   */
  std::string getLocalUsername() { return ufrag_; }
  std::string getLocalPassword() { return upass_; }

  /**
   * Set remote credentials
   * @param username and password
   */
  void setRemoteCredentials(const std::string& username, const std::string& password);

  /**
   * Sets the associated Listener.
   * @param connection Pointer to the NiceConnectionListener.
   */
  void setNiceListener(NiceConnectionListener *listener);
  /**
   * Gets the associated Listener.
   * @param connection Pointer to the NiceConnectionListener.
   */
  NiceConnectionListener* getNiceListener();
  /**
   * Sends data via the ICE Connection.
   * @param buf Pointer to the data buffer.
   * @param len Length of the Buffer.
   * @return Bytes sent.
   */
  int sendData(unsigned int compId, const void* buf, int len);

  void updateIceState(IceState state);
  IceState checkIceState();
  void updateComponentState(unsigned int compId, IceState state);
  void queueData(unsigned int component_id, char* buf, int len);
  CandidatePair getSelectedPair();
  packetPtr getPacket();
  void setReceivedLastCandidate(bool hasReceived);
  void close();

 private:
  std::string connection_id_;
  NiceAgent* agent_;
  GMainContext* context_;
  GMainLoop* loop_;

  NiceConnectionListener* listener_;
  std::queue<packetPtr> niceQueue_;
  unsigned int candsDelivered_;
  IceState iceState_;

  boost::thread m_Thread_;
  boost::mutex queueMutex_, closeMutex_;
  boost::condition_variable cond_;

  unsigned int iceComponents_;
  std::map <unsigned int, IceState> comp_state_list_;
  std::string ufrag_, upass_, username_, password_;
  IceConfig iceConfig_;
  bool receivedLastCandidate_;

  void mainLoop();

  inline const char* toLog() {
    return (std::string("id: ")+connection_id_).c_str();
  }
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_NICECONNECTION_H_
