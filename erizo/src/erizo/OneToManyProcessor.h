/*
* OneToManyProcessor.h
*/

#ifndef ERIZO_SRC_ERIZO_ONETOMANYPROCESSOR_H_
#define ERIZO_SRC_ERIZO_ONETOMANYPROCESSOR_H_

#include <map>
#include <string>
#include <boost/thread/future.hpp>

#include "./MediaDefinitions.h"
#include "media/ExternalOutput.h"
#include "./logger.h"

namespace erizo {

class MediaStream;

/**
* Represents a One to Many connection.
* Receives media from one publisher and retransmits it to every subscriber.
*/
class OneToManyProcessor
    : public MediaSink, public FeedbackSink, public std::enable_shared_from_this<OneToManyProcessor> {
  DECLARE_LOGGER();

 public:
  OneToManyProcessor();
  virtual ~OneToManyProcessor();
  /**
  * Sets the Publisher
  * @param webRtcConn The MediaStream of the Publisher
  */
  void setPublisher(std::shared_ptr<MediaSource> publisher_stream, std::string publisher_id);

  std::shared_ptr<MediaSource> getPublisher();
  /**
  * Sets the subscriber
  * @param webRtcConn The MediaStream of the subscriber
  * @param peerId An unique Id for the subscriber
  */
  void addSubscriber(std::shared_ptr<MediaSink> subscriber_stream, const std::string& peer_id);

  std::shared_ptr<MediaSink> getSubscriber(const std::string& peer_id);

  /**
  * Eliminates the subscriber given its peer id
  * @param peerId the peerId
  */
  void removeSubscriber(const std::string& peer_id);

  boost::future<void> close() override;

 private:
  int deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) override;
  int deliverVideoData_(std::shared_ptr<DataPacket> video_packet) override;
  int deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) override;
  int deliverEvent_(MediaEventPtr event) override;
  boost::future<void> closeAll();
  bool isSSRCFromAudio(uint32_t ssrc);
  uint32_t translateAndMaybeAdaptForSimulcast(uint32_t orig_ssrc);

 private:
  std::weak_ptr<FeedbackSink> feedback_sink_;
  std::map<std::string, std::shared_ptr<MediaSink>> subscribers_;
  std::shared_ptr<MediaSource> publisher_;
  std::string publisher_id_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_ONETOMANYPROCESSOR_H_
