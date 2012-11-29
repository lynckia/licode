#include <OneToManyProcessor.h>

class SDPReceiver {

public:

	SDPReceiver();
	virtual ~SDPReceiver(){};
	bool createPublisher(int peer_id);
	bool createSubscriber(int peer_id);
	void setRemoteSDP(int peer_id, const std::string &sdp);
	std::string getLocalSDP(int peer_id);
	void peerDisconnected(int peer_id);

private:

	erizo::OneToManyProcessor* muxer;
};
