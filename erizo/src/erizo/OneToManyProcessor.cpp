/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"

#include <map>
#include <string>

#include "./MediaStream.h"
#include "rtp/RtpHeaders.h"
#include "rtp/RtpUtils.h"

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
    RtpHeader* head = reinterpret_cast<RtpHeader*>(audio_packet->data);
    RtcpHeader* chead = reinterpret_cast<RtcpHeader*>(audio_packet->data);
    for (it = subscribers.begin(); it != subscribers.end(); ++it) {
      if ((*it).second != nullptr) {
        // Hack to avoid audio drifting in Chrome.
        if (chead->isRtcp() && chead->isSDES()) {
          chead->setSSRC((*it).second->getAudioSinkSSRC());
        } else {
          head->setSSRC((*it).second->getAudioSinkSSRC());
        }
        // Note: deliverAudioData must copy the packet inmediately
        (*it).second->deliverAudioData(audio_packet);
      }
    }

    return 0;
  }

  bool OneToManyProcessor::isSSRCFromAudio(uint32_t ssrc) {
    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); ++it) {
      if ((*it).second->getAudioSinkSSRC() == ssrc) {
        return true;
      }
    }
    return false;
  }

  int OneToManyProcessor::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
    if (video_packet->length <= 0)
      return 0;
    RtcpHeader* head = reinterpret_cast<RtcpHeader*>(video_packet->data);
    if (head->isFeedback()) {
      ELOG_WARN("Receiving Feedback in wrong path: %d", head->packettype);
      deliverFeedback_(video_packet);
      return 0;
    }
    boost::unique_lock<boost::mutex> lock(monitor_mutex_);
    if (subscribers.empty())
      return 0;
    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it;
    RtpHeader* rhead = reinterpret_cast<RtpHeader*>(video_packet->data);
    uint32_t ssrc = head->isRtcp() ? head->getSSRC() : rhead->getSSRC();
    uint32_t ssrc_offset = translateAndMaybeAdaptForSimulcast(ssrc);
    for (it = subscribers.begin(); it != subscribers.end(); ++it) {
      if ((*it).second != nullptr) {
        uint32_t base_ssrc = (*it).second->getVideoSinkSSRC();
        if (head->isRtcp()) {
          head->setSSRC(base_ssrc + ssrc_offset);
        } else {
          rhead->setSSRC(base_ssrc + ssrc_offset);
        }
        // Note: deliverVideoData must copy the packet inmediately
        (*it).second->deliverVideoData(video_packet);
      }
    }
    return 0;
  }

  uint32_t OneToManyProcessor::translateAndMaybeAdaptForSimulcast(uint32_t orig_ssrc) {
    return orig_ssrc - publisher->getVideoSourceSSRC();
  }

  void OneToManyProcessor::setPublisher(std::shared_ptr<MediaSource> publisher_stream) {
    boost::mutex::scoped_lock lock(monitor_mutex_);
    this->publisher = publisher_stream;
    feedbackSink_ = publisher->getFeedbackSink();
  }

  int OneToManyProcessor::deliverFeedback_(std::shared_ptr<DataPacket> fb_packet) {
    if (feedbackSink_ != nullptr) {
      RtpUtils::forEachRtcpBlock(fb_packet, [this](RtcpHeader *chead) {
        if (chead->isREMB()) {
          for (uint8_t index = 0; index < chead->getREMBNumSSRC(); index++) {
            if (isSSRCFromAudio(chead->getREMBFeedSSRC(index))) {
              chead->setREMBFeedSSRC(index, publisher->getAudioSourceSSRC());
            } else {
              chead->setREMBFeedSSRC(index, publisher->getVideoSourceSSRC());
            }
          }
        }
        if (isSSRCFromAudio(chead->getSourceSSRC())) {
          chead->setSourceSSRC(publisher->getAudioSourceSSRC());
        } else {
          chead->setSourceSSRC(publisher->getVideoSourceSSRC());
        }
      });
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

  boost::future<void> OneToManyProcessor::close() {
    return closeAll();
  }

  boost::future<void> OneToManyProcessor::closeAll() {
    ELOG_DEBUG("OneToManyProcessor closeAll");
    std::shared_ptr<boost::promise<void>> p = std::make_shared<boost::promise<void>>();
    boost::future<void> f = p->get_future();
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
    p->set_value();
    ELOG_DEBUG("ClosedAll media in this OneToMany");
    return f;
  }

}  // namespace erizo
