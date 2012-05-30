/*
 * OneToManyProcessor.h
 *
 *  Created on: Mar 21, 2012
 *      Author: pedro
 */

#ifndef ONETOMANYPROCESSOR_H_
#define ONETOMANYPROCESSOR_H_

#include <map>

#include "MediaDefinitions.h"

namespace erizo{

class WebRtcConnection;

class OneToManyProcessor : public MediaReceiver {
public:
	OneToManyProcessor();
	virtual ~OneToManyProcessor();
	void setPublisher(WebRtcConnection* webRtcConn);
	void addSubscriber(WebRtcConnection* webRtcConn, int peerId);
	void removeSubscriber(int peerId);
	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);

	WebRtcConnection *publisher;
	std::map<int, WebRtcConnection*> subscribers;

private:
	char* sendVideoBuffer_;
	char* sendAudioBuffer_;
};

} /* namespace erizo */
#endif /* ONETOMANYPROCESSOR_H_ */
