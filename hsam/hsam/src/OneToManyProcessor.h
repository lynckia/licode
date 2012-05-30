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

class WebRTCConnection;
class OneToManyProcessor : public MediaReceiver {
public:
	OneToManyProcessor();
	virtual ~OneToManyProcessor();
	void setPublisher(WebRTCConnection* conn);
	void addSubscriber(WebRTCConnection* conn, int peer_id);
	void removeSubscriber(int peer_id);
	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);
	WebRTCConnection *publisher;
	std::map<int, WebRTCConnection*> subscribers;
	char* sendVideoBuffer;
	char* sendAudioBuffer;


private:
};

#endif /* ONETOMANYPROCESSOR_H_ */
