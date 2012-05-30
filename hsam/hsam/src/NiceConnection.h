/*
 * NiceConnection.h
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#ifndef NICECONNECTION_H_
#define NICECONNECTION_H_

#include <nice/nice.h>
//#include <vector>
#include <string>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "MediaDefinitions.h"
#include "SdpInfo.h"

//forward declarations
struct CandidateInfo;
class WebRTCConnection;


class NiceConnection {

public:

	enum IceState{
		INITIAL,
		CANDIDATES_GATHERED,
		CANDIDATES_RECEIVED,
		READY,
		FINISHED
	};

	mediaType media_type;
	std::string *trans_name;
	IceState state;
	NiceConnection(mediaType med, const std::string &transport_name);
	virtual ~NiceConnection();
	void join();
	void start();
	void close();
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates);
	void setWebRTCConnection(WebRTCConnection *connection);
	WebRTCConnection* getWebRTCConnection();
	std::vector<CandidateInfo> *local_candidates;
	int sendData(void* buf, int len);



private:
	void init();
//	std::string getLocalAddress();
	NiceAgent* agent;
	WebRTCConnection* conn;
	GMainLoop* loop;
	boost::thread m_Thread;


};

#endif /* NICECONNECTION_H_ */
