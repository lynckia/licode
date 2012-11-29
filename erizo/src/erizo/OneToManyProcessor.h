/*
 * OneToManyProcessor.h
 */

#ifndef ONETOMANYPROCESSOR_H_
#define ONETOMANYPROCESSOR_H_

#include <map>

#include "MediaDefinitions.h"

namespace erizo{

class WebRtcConnection;

/**
 * Represents a One to Many connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class OneToManyProcessor : public MediaReceiver {
public:
	WebRtcConnection *publisher;
	std::map<int, WebRtcConnection*> subscribers;

	OneToManyProcessor();
	virtual ~OneToManyProcessor();
	/**
	 * Sets the Publisher
	 * @param webRtcConn The WebRtcConnection of the Publisher
	 */
	void setPublisher(WebRtcConnection* webRtcConn);
	/**
	 * Sets the subscriber
	 * @param webRtcConn The WebRtcConnection of the subscriber
	 * @param peerId An unique Id for the subscriber
	 */
	void addSubscriber(WebRtcConnection* webRtcConn, int peerId);
	/**
	 * Eliminates the subscriber given its peer id
	 * @param peerId the peerId
	 */
	void removeSubscriber(int peerId);
	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);
	/**
	 * Closes all the subscribers and the publisher, the object is useless after this
	 */
	void closeAll();

private:
	char* sendVideoBuffer_;
	char* sendAudioBuffer_;
	unsigned int sentPackets_;
};

} /* namespace erizo */
#endif /* ONETOMANYPROCESSOR_H_ */
