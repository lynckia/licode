/*
 * NiceConnection.h
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#ifndef NICECONNECTION_H_
#define NICECONNECTION_H_

#include <nice/nice.h>
#include "mediadefinitions.h"
#include "sdpinfo.h"
#include <vector>
#include <string>
#include <boost/thread.hpp>

//forward declaration
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
	mediaType type;
	IceState state;
	NiceConnection(const std::string &localaddr, const std::string &stunaddr);
	virtual ~NiceConnection();
	void join();
	void start();
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates);
	void setWebRTCConnection(WebRTCConnection *connection);
	std::vector<CandidateInfo> localCandidates;
	int sendData(void* buf, int len);
	WebRTCConnection* conn;


private:
	void init();

	NiceAgent* agent;
	GMainLoop* loop;
	boost::thread m_Thread;

};

#endif /* NICECONNECTION_H_ */
