/*
 * NiceConnection.h
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#ifndef NICECONNECTION_H_
#define NICECONNECTION_H_

#include <nice/nice.h>
#include <vector>
#include <string>

//forward declaration
struct CandidateInfo;



class NiceConnection {

public:
	enum IceState{
		INITIAL,
		CANDIDATES_GATHERED,
		CANDIDATES_RECEIVED,
		READY,
		FINISHED
	};
	IceState state;
	NiceConnection(const std::string &localaddr, const std::string &stunaddr);
	virtual ~NiceConnection();
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates);
	std::vector<CandidateInfo> localCandidates;

private:
	NiceAgent* agent;
	GMainLoop* loop;
//	void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer user_data);

};

#endif /* NICECONNECTION_H_ */
