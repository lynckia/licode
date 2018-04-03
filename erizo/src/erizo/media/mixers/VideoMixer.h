
/*
* VideoMixer.h
*/
#ifndef ERIZO_SRC_ERIZO_MEDIA_MIXERS_VIDEOMIXER_H_
#define ERIZO_SRC_ERIZO_MEDIA_MIXERS_VIDEOMIXER_H_

#include <map>
#include <vector>

#include "./MediaDefinitions.h"
#include "media/MediaProcessor.h"

#include "./logger.h"

namespace erizo {
class WebRtcConnection;
class RTPSink;

class VideoMixer : public MediaSink, public RawDataReceiver, public RTPDataReceiver {
  DECLARE_LOGGER();

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
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override;
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override;

  void receiveRawData(const RawDataPacket& packet) override;
  void receiveRtpData(unsigned char* rtpdata, int len) override;

  void closeSink();

  // MediaProcessor *mp;
  InputProcessor* ip;
  OutputProcessor* op;
  /**
  * Closes all the subscribers and the publisher, the object is useless after this
  */
  void closeAll();

 private:
  char sendVideoBuffer_[2000];
  char sendAudioBuffer_[2000];
  RTPSink* sink_;
  std::vector<DataPacket> head;
  int gotFrame_, gotDecodedFrame_, size_;
  void sendHead(WebRtcConnection* conn);
  RtpVP8Parser pars;
  unsigned int sentPackets_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_MEDIA_MIXERS_VIDEOMIXER_H_
