/*
 * OneToManyTranscoder.h
 */

#ifndef ONETOMANYTRANSCODER_H_
#define ONETOMANYTRANSCODER_H_

#include <map>
#include <vector>

#include "../MediaDefinitions.h"
#include "MediaProcessor.h"


namespace erizo{
class WebRtcConnection;
class RTPSink;

/**
 * Represents a One to Many connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class OneToManyTranscoder : public MediaSink, public RawDataReceiver, public RTPDataReceiver {
public:
	MediaSource* publisher;
	std::map<std::string, MediaSink*> subscribers;

	OneToManyTranscoder();
	virtual ~OneToManyTranscoder();
	/**
	 * Sets the Publisher
	 * @param webRtcConn The WebRtcConnection of the Publisher
	 */
	void setPublisher(MediaSource* webRtcConn);
	/**
	 * Sets the subscriber
	 * @param webRtcConn The WebRtcConnection of the subscriber
	 * @param peerId An unique Id for the subscriber
	 */
	void addSubscriber(MediaSink* webRtcConn, const std::string& peerId);
	/**
	 * Eliminates the subscriber given its peer id
	 * @param peerId the peerId
	 */
	void removeSubscriber(const std::string& peerId);
	int deliverAudioData(char* buf, int len);
	int deliverVideoData(char* buf, int len);
	void receiveRawData(RawDataPacket& packet);
	void receiveRtpData(unsigned char*rtpdata, int len);

//	MediaProcessor *mp;
	InputProcessor* ip;
	OutputProcessor* op;
  
  void closeSink();
  void close();
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
#endif /* ONETOMANYTRANSCODER_H_ */
