/*
 * NiceConnection.h
 */

#ifndef NICECONNECTION_H_
#define NICECONNECTION_H_

#include <string>
#include <boost/thread.hpp>

#include "MediaDefinitions.h"
#include "SdpInfo.h"

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;

namespace erizo {
//forward declarations
struct CandidateInfo;
class WebRtcConnection;

class NiceConnection {
public:
	enum IceState{
		INITIAL,
		CANDIDATES_GATHERED,
		CANDIDATES_RECEIVED,
		READY,
		FINISHED
	};

	NiceConnection(MediaType med, const std::string &transportName);
	virtual ~NiceConnection();
	void join();
	void start();
	void close();
	WebRtcConnection* getWebRtcConnection();
	bool setRemoteCandidates(std::vector<CandidateInfo> &candidates);
	void setWebRtcConnection(WebRtcConnection *connection);
	int sendData(void* buf, int len);

	MediaType mediaType;
	std::string *transportName;
	IceState iceState;
	std::vector<CandidateInfo> *localCandidates;

private:
	void init();
	NiceAgent* agent_;
	WebRtcConnection* conn_;
	GMainLoop* loop_;
	boost::thread m_Thread_;
};

} /* namespace erizo */
#endif /* NICECONNECTION_H_ */
