/*
 * NiceConnection.h
 */

#ifndef NICECONNECTION_H_
#define NICECONNECTION_H_

#include <string>
#include <vector>
#include <queue>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

#include "MediaDefinitions.h"
#include "SdpInfo.h"
#include "logger.h"

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainContext GMainContext;

typedef unsigned int uint;

namespace erizo {

#define NICE_STREAM_MAX_UFRAG   256 + 1  /* ufrag + NULL */
#define NICE_STREAM_MAX_UNAME   256 * 2 + 1 + 1 /* 2*ufrag + colon + NULL */
#define NICE_STREAM_MAX_PWD     256 + 1  /* pwd + NULL */
#define NICE_STREAM_DEF_UFRAG   4 + 1    /* ufrag + NULL */
#define NICE_STREAM_DEF_PWD     22 + 1   /* pwd + NULL */

//forward declarations
typedef boost::shared_ptr<dataPacket> packetPtr;
class CandidateInfo;
class WebRtcConnection;

struct CandidatePair{
  std::string erizoCandidateIp;
  int erizoCandidatePort;
  std::string clientCandidateIp;
  int clientCandidatePort;
};
/**
 * States of ICE
 */
enum IceState {
	NICE_INITIAL, NICE_CANDIDATES_RECEIVED, NICE_READY, NICE_FINISHED, NICE_FAILED
};

class NiceConnectionListener {
public:
	virtual void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* conn)=0;
	virtual void onCandidate(const CandidateInfo &candidate, NiceConnection *conn)=0;
	virtual void updateIceState(IceState state, NiceConnection *conn)=0;
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
	NiceConnection(MediaType med, const std::string &transportName, NiceConnectionListener* listener, unsigned int iceComponents=1,
		const std::string& stunServer = "", int stunPort = 3478, int minPort = 0, int maxPort = 65535, std::string username = "", std::string password = "");

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
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates, bool isBundle);
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
	 * @param username and password where credentials will be stored
	 */
	void getLocalCredentials(std::string& username, std::string& password);
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
  void close();

private:
	void init();
	NiceAgent* agent_;
	NiceConnectionListener* listener_;
  	std::queue<packetPtr> niceQueue_;
  	unsigned int candsDelivered_;

	GMainContext* context_;
	boost::thread m_Thread_;
	IceState iceState_;
  	boost::mutex queueMutex_;
	boost::condition_variable cond_;
  	unsigned int iceComponents_;
  	std::map <unsigned int, IceState> comp_state_list_;
  	bool running_;
	std::string ufrag_, upass_;
};

} /* namespace erizo */
#endif /* NICECONNECTION_H_ */
