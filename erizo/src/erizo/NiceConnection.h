/*
 * NiceConnection.h
 */

#ifndef NICECONNECTION_H_
#define NICECONNECTION_H_

#include <string>
#include <vector>
#include <queue>
#include <boost/thread.hpp>

#include "MediaDefinitions.h"
#include "SdpInfo.h"
#include "logger.h"

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;
typedef struct _GMainContext GMainContext;

typedef unsigned int uint;

namespace erizo {
//forward declarations
typedef boost::shared_ptr<dataPacket> packetPtr;
class CandidateInfo;
class WebRtcConnection;

/**
 * States of ICE
 */
enum IceState {
	NICE_INITIAL, NICE_CANDIDATES_GATHERED, NICE_CANDIDATES_RECEIVED, NICE_READY, NICE_FINISHED, NICE_FAILED
};

class NiceConnectionListener {
public:
	virtual void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* conn)=0;
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
		const std::string& stunServer = "", int stunPort = 3478, int minPort = 0, int maxPort = 65535);

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
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates);
	/**
	 * Sets the local ICE Candidates. Called by C Nice functions.
	 * @param candidates A vector containing the CandidateInfo.
	 * @return true if successfull.
	 */
	void gatheringDone(uint stream_id);

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

  packetPtr getPacket();
  void close();

private:
	void init();
	NiceAgent* agent_;
	NiceConnectionListener* listener_;
  std::queue<packetPtr> niceQueue_;
	GMainLoop* loop_;
	GMainContext* context_;
	boost::thread m_Thread_;
	IceState iceState_;
	boost::mutex queueMutex_, agentMutex_;
  boost::recursive_mutex stateMutex_;
	boost::condition_variable cond_;
  unsigned int iceComponents_;
  std::map <unsigned int, IceState> comp_state_list_;
	std::string stunServer_;
  bool running_;
	int stunPort_, minPort_, maxPort_;
};

} /* namespace erizo */
#endif /* NICECONNECTION_H_ */
