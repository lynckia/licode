/*
* OneToManyTranscoder.h
*/

#ifndef ERIZO_SRC_ERIZO_MEDIA_ONETOMANYTRANSCODER_H_
#define ERIZO_SRC_ERIZO_MEDIA_ONETOMANYTRANSCODER_H_

#include <map>
#include <string>
#include <vector>

#include "./MediaDefinitions.h"
#include "media/MediaProcessor.h"
#include "./logger.h"

namespace erizo {
class MediaStream;
class RTPSink;

/**
* Represents a One to Many connection.
* Receives media from one publisher and retransmits it to every subscriber.
*/
class OneToManyTranscoder : public MediaSink, public RawDataReceiver, public RTPDataReceiver {
  DECLARE_LOGGER();

 public:
  MediaSource* publisher;
  std::map<std::string, MediaSink*> subscribers;

  OneToManyTranscoder();
  virtual ~OneToManyTranscoder();
  /**
  * Sets the Publisher
  * @param webRtcConn The MediaStream of the Publisher
  */
  void setPublisher(MediaSource* media_stream);
  /**
  * Sets the subscriber
  * @param webRtcConn The MediaStream of the subscriber
  * @param peerId An unique Id for the subscriber
  */
  void addSubscriber(MediaSink* media_stream, const std::string& peer_id);
  /**
  * Eliminates the subscriber given its peer id
  * @param peerId the peerId
  */
  void removeSubscriber(const std::string& peerId);
  void receiveRawData(const RawDataPacket& packet) override;
  void receiveRtpData(unsigned char*rtpdata, int len) override;

  // MediaProcessor *mp;
  InputProcessor* ip_;
  OutputProcessor* op_;

 private:
  char sendVideoBuffer_[2000];
  char sendAudioBuffer_[2000];
  RTPSink* sink_;
  std::vector<DataPacket> head;
  int gotFrame_, gotDecodedFrame_, size_;
  void sendHead(MediaStream* conn);
  RtpVP8Parser pars;
  unsigned int sentPackets_;
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override;
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override;
  int deliverEvent_(MediaEventPtr event) override;
  /**
  * Closes all the subscribers and the publisher, the object is useless after this
  */
  void closeAll();
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_ONETOMANYTRANSCODER_H_
