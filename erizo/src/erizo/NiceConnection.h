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
#include "WebRtcConnection.h"

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;

namespace erizo {
//forward declarations
struct CandidateInfo;
class WebRtcConnection;
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
	NiceConnection(MediaType med, const std::string &transportName, int iceComponents=1);
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
	 * Obtains the associated WebRtcConnection.
	 * @return A pointer to the WebRtcConnection.
	 */
	WebRtcConnection* getWebRtcConnection();
	/**
	 * Sets the remote ICE Candidates.
	 * @param candidates A vector containing the CandidateInfo.
	 * @return true if successfull.
	 */
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates);
	/**
	 * Sets the associated WebRTCConnection.
	 * @param connection Pointer to the WebRtcConenction.
	 */
	void setWebRtcConnection(WebRtcConnection *connection);
	/**
	 * Sends data via the ICE Connection.
	 * @param buf Pointer to the data buffer.
	 * @param len Length of the Buffer.
	 * @return Bytes sent.
	 */
	int sendData(const void* buf, int len);

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


private:
	void init();
	NiceAgent* agent_;
	WebRtcConnection* conn_;
	GMainLoop* loop_;
	boost::thread m_Thread_;
  int iceComponents_;

};

} /* namespace erizo */
#endif /* NICECONNECTION_H_ */
