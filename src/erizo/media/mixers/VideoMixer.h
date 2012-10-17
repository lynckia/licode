
/*
 * VideoMixer.h
 */

#ifndef VIDEOMIXER_H_
#define VIDEOMIXER_H_

#include <map>
#include <vector>

#include "../../MediaDefinitions.h"
#include "../MediaProcessor.h"


namespace erizo{
class WebRtcConnection;
class RTPSink;

/**
 * Represents a One to Many connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class VideoMixer : public MediaReceiver, public RawDataReceiver, public RTPDataReceiver {
public:
	WebRtcConnection *publisher;
	std::map<int, WebRtcConnection*> subscribers;

	VideoMixer();
	virtual ~VideoMixer();
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
	void receiveRawData(RawDataPacket& packet);
	void receiveRtpData(unsigned char*rtpdata, int len);

//	MediaProcessor *mp;
	InputProcessor* ip;
	OutputProcessor* op;
	/**
	 * Closes all the subscribers and the publisher, the object is useless after this
	 */
	void closeAll();

private:
	char* sendVideoBuffer_;
	char* sendAudioBuffer_;
	char* unpackagedBuffer_;
	char* decodedBuffer_;
	char* codedBuffer_;
	RTPSink* sink_;
	std::vector<packet> head;
	int gotFrame_,gotDecodedFrame_, size_;
	void sendHead(WebRtcConnection* conn);
	RtpParser pars;
	unsigned int sentPackets_;
};

} /* namespace erizo */
#endif /* VIDEOMIXER_H_ */
