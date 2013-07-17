/*
 * NiceConnection.h
 */

#ifndef NICECONNECTION_H_
#define NICECONNECTION_H_

#include <string>
#include <vector>
#include <boost/thread.hpp>

#include "MediaDefinitions.h"
#include "SdpInfo.h"

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;

namespace erizo {
//forward declarations
struct CandidateInfo;
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
public:

	/**
	 * Constructs a new NiceConnection.
	 * @param med The MediaType of the connection.
	 * @param transportName The name of the transport protocol. Was used when WebRTC used video_rtp instead of just rtp.
   * @param iceComponents Number of ice components pero connection. Default is 1 (rtcp-mux).
	 */
	NiceConnection(MediaType med, const std::string &transportName, unsigned int iceComponents=1);
	virtual ~NiceConnection();
	/**
	 * Join to the internal thread of the NiceConnection.
	 */
	void join();
	/**
	 * Starts Gathering candidates in a new thread.
	 */
	void start();
	/**
	 * Closes the connection. It renders the object unusable.
	 */
	void close();
	/**
	 * Sets the remote ICE Candidates.
	 * @param candidates A vector containing the CandidateInfo.
	 * @return true if successfull.
	 */
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates);
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

	/**
	 * The MediaType of the connection
	 */
	MediaType mediaType;
	/**
	 * The transport name
	 */
	std::string *transportName;
	/**
	 * The state of the ice Connection
	 */
	IceState iceState;
	/**
	 * The Obtained local candidates.
	 */
	std::vector<CandidateInfo>* localCandidates;

	void updateIceState(IceState state);
	void updateComponentState(unsigned int compId, IceState state);


private:
	void init();
	NiceAgent* agent_;
	NiceConnectionListener* listener_;
	GMainLoop* loop_;
	boost::thread m_Thread_;
  	unsigned int iceComponents_;
  	std::map <uint, IceState> comp_state_list;
};

} /* namespace erizo */
#endif /* NICECONNECTION_H_ */
