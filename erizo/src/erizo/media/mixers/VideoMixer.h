
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
class VideoMixer : public MediaSink, public RawDataReceiver, public RTPDataReceiver {
public:
	WebRtcConnection *subscriber;
	std::map<int, WebRtcConnection*> publishers;

	VideoMixer();
	virtual ~VideoMixer();
	/**
	 * Sets the Publisher
	 * @param webRtcConn The WebRtcConnection of the Publisher
	 */
	void addPublisher(WebRtcConnection* webRtcConn, int peerSSRC);
	/**
	 * Sets the subscriber
	 * @param webRtcConn The WebRtcConnection of the subscriber
	 * @param peerId An unique Id for the subscriber
	 */
	void setSubscriber(WebRtcConnection* webRtcConn);
	/**
	 * Eliminates the subscriber given its peer id
	 * @param peerId the peerId
	 */
	void removePublisher(int peerSSRC);
	int deliverAudioData(char* buf, int len);
	int deliverVideoData(char* buf, int len);
	void receiveRawData(RawDataPacket& packet);
  void receiveRtpData(unsigned char* rtpdata, int len);

  void closeSink();

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
	std::vector<dataPacket> head;
	int gotFrame_,gotDecodedFrame_, size_;
	void sendHead(WebRtcConnection* conn);
	RtpParser pars;
	unsigned int sentPackets_;
};

} /* namespace erizo */
#endif /* VIDEOMIXER_H_ */
