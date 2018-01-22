/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"

#include <map>
#include <string>

#include "./MediaStream.h"
#include "rtp/RtpHeaders.h"

namespace erizo {
  DEFINE_LOGGER(OneToManyProcessor, "OneToManyProcessor");
  OneToManyProcessor::OneToManyProcessor() : feedbackSink_{nullptr} {
    ELOG_DEBUG("OneToManyProcessor constructor");
  }

  OneToManyProcessor::~OneToManyProcessor() {
  }

  int OneToManyProcessor::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
    if (audio_packet->length <= 0)
      return 0;

    boost::unique_lock<boost::mutex> lock(monitor_mutex_);
    if (subscribers.empty())
      return 0;

    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); ++it) {
      if ((*it).second != nullptr) {
        (*it).second->deliverAudioData(audio_packet);
      }
    }

    return 0;
  }

  int OneToManyProcessor::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
    if (video_packet->length <= 0)
      return 0;
    RtcpHeader* head = reinterpret_cast<RtcpHeader*>(video_packet->data);
    if (head->isFeedback()) {
      ELOG_WARN("Receiving Feedback in wrong path: %d", head->packettype);
      if (feedbackSink_ != nullptr) {
        head->ssrc = htonl(publisher->getVideoSourceSSRC());
        feedbackSink_->deliverFeedback(video_packet);
      }
      return 0;
    }
    boost::unique_lock<boost::mutex> lock(monitor_mutex_);
    if (subscribers.empty())
      return 0;
    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); ++it) {
      if ((*it).second != nullptr) {
        (*it).second->deliverVideoData(video_packet);
      }
    }
    return 0;
  }

  void OneToManyProcessor::setPublisher(std::shared_ptr<MediaSource> publisher_stream) {
    boost::mutex::scoped_lock lock(monitor_mutex_);
    this->publisher = publisher_stream;
    feedbackSink_ = publisher->getFeedbackSink();
  }

  int OneToManyProcessor::deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) {
    if (feedbackSink_ != nullptr) {
      feedbackSink_->deliverFeedback(fb_packet);
    }
    return 0;
  }

  int OneToManyProcessor::deliverEvent_(MediaEventPtr event) {
    boost::unique_lock<boost::mutex> lock(monitor_mutex_);
    if (subscribers.empty())
      return 0;
    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); ++it) {
      if ((*it).second != nullptr) {
        (*it).second->deliverEvent(event);
      }
    }
    return 0;
  }

  void OneToManyProcessor::addSubscriber(std::shared_ptr<MediaSink> subscriber_stream,
      const std::string& peer_id) {
    ELOG_DEBUG("Adding subscriber");
    boost::mutex::scoped_lock lock(monitor_mutex_);
    ELOG_DEBUG("From %u, %u ", publisher->getAudioSourceSSRC(), publisher->getVideoSourceSSRC());
    subscriber_stream->setAudioSinkSSRC(this->publisher->getAudioSourceSSRC());
    subscriber_stream->setVideoSinkSSRC(this->publisher->getVideoSourceSSRC());
    ELOG_DEBUG("Subscribers ssrcs: Audio %u, video, %u from %u, %u ",
               subscriber_stream->getAudioSinkSSRC(), subscriber_stream->getVideoSinkSSRC(),
               this->publisher->getAudioSourceSSRC() , this->publisher->getVideoSourceSSRC());
    FeedbackSource* fbsource = subscriber_stream->getFeedbackSource();

    if (fbsource != nullptr) {
      ELOG_DEBUG("adding fbsource");
      fbsource->setFeedbackSink(this);
    }
    if (this->subscribers.find(peer_id) != subscribers.end()) {
        ELOG_WARN("This OTM already has a subscriber with peer_id %s, substituting it", peer_id.c_str());
        this->subscribers.erase(peer_id);
    }
    this->subscribers[peer_id] = subscriber_stream;
  }

  void OneToManyProcessor::removeSubscriber(const std::string& peer_id) {
    ELOG_DEBUG("Remove subscriber %s", peer_id.c_str());
    boost::mutex::scoped_lock lock(monitor_mutex_);
    if (this->subscribers.find(peer_id) != subscribers.end()) {
      this->subscribers.erase(peer_id);
    }
  }

  void OneToManyProcessor::close() {
    closeAll();
  }

  void OneToManyProcessor::closeAll() {
    ELOG_DEBUG("OneToManyProcessor closeAll");
    feedbackSink_ = nullptr;
    publisher.reset();
    boost::unique_lock<boost::mutex> lock(monitor_mutex_);
    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it = subscribers.begin();
    while (it != subscribers.end()) {
      if ((*it).second != nullptr) {
        FeedbackSource* fbsource = (*it).second->getFeedbackSource();
        if (fbsource != nullptr) {
          fbsource->setFeedbackSink(nullptr);
        }
      }
      subscribers.erase(it++);
    }
    subscribers.clear();
    ELOG_DEBUG("ClosedAll media in this OneToMany");
  }

}  // namespace erizo
