/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"
#include "WebRtcConnection.h"
#include "rtputils.h"

namespace erizo {
  DEFINE_LOGGER(OneToManyProcessor, "OneToManyProcessor");
  OneToManyProcessor::OneToManyProcessor() {

    sendVideoBuffer_ = (char*) malloc(20000);
    sendAudioBuffer_ = (char*) malloc(20000);
    publisher = NULL;
    feedbackSink_ = NULL;
    sentPackets_ = 0;

  }

  OneToManyProcessor::~OneToManyProcessor() {
    ELOG_WARN ("OneToManyProcessor destructor");
    this->closeAll();
    if (sendVideoBuffer_!=NULL)
      free(sendVideoBuffer_);
    if (sendAudioBuffer_!=NULL)
      free(sendAudioBuffer_);
  }

  int OneToManyProcessor::deliverAudioData(char* buf, int len) {
    if (subscribers.empty() || len <= 0)
      return 0;

    std::map<std::string, MediaSink*>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      (*it).second->deliverAudioData(buf, len);
    }

    return 0;
  }

  int OneToManyProcessor::deliverVideoData(char* buf, int len) {
    if (subscribers.empty() || len <= 0)
      return 0;

    rtcpheader* head = reinterpret_cast<rtcpheader*>(buf);
    if(head->packettype==RTCP_Receiver_PT || head->packettype==RTCP_PS_Feedback_PT|| head->packettype == RTCP_RTP_Feedback_PT){
      ELOG_WARN("Receiving Feedback in wrong path: %d", head->packettype);
      if (feedbackSink_){
        head->ssrc = htonl(publisher->getVideoSourceSSRC());
        feedbackSink_->deliverFeedback(buf,len);
      }
      return 0;
    }
    std::map<std::string, MediaSink*>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      (*it).second->deliverVideoData(buf, len);
    }
    sentPackets_++;
    return 0;
  }

  void OneToManyProcessor::setPublisher(MediaSource* webRtcConn) {
    ELOG_DEBUG("SET PUBLISHER");
    this->publisher = webRtcConn;
    feedbackSink_ = publisher->getFeedbackSink();
  }

  int OneToManyProcessor::deliverFeedback(char* buf, int len){
    if (feedbackSink_ != NULL) {
      feedbackSink_->deliverFeedback(buf,len);
    }
    return 0;

  }

  void OneToManyProcessor::addSubscriber(MediaSink* webRtcConn,
      const std::string& peerId) {
    ELOG_DEBUG("Adding subscriber");
    ELOG_DEBUG("From %u, %u ", publisher->getAudioSourceSSRC() , publisher->getVideoSourceSSRC());
    webRtcConn->setAudioSinkSSRC(this->publisher->getAudioSourceSSRC());
    webRtcConn->setVideoSinkSSRC(this->publisher->getVideoSourceSSRC());
    ELOG_DEBUG("Subscribers ssrcs: Audio %u, video, %u from %u, %u ", webRtcConn->getAudioSinkSSRC(), webRtcConn->getVideoSinkSSRC(), this->publisher->getAudioSourceSSRC() , this->publisher->getVideoSourceSSRC());
    FeedbackSource* fbsource = webRtcConn->getFeedbackSource();

    if (fbsource!=NULL){
      ELOG_DEBUG("adding fbsource");
      fbsource->setFeedbackSink(this);
    }
    this->subscribers[peerId] = webRtcConn;
  }

  void OneToManyProcessor::removeSubscriber(const std::string& peerId) {
    if (this->subscribers.find(peerId) != subscribers.end()) {
      this->subscribers[peerId]->closeSink();
      this->subscribers.erase(peerId);
    }
  }

  void OneToManyProcessor::closeSink(){
    ELOG_WARN ("OneToManyProcessor closeSink");
    this->close();
  }

  void OneToManyProcessor::close(){
    ELOG_WARN ("OneToManyProcessor close");
    this->closeAll();
  }

  void OneToManyProcessor::closeAll() {
    ELOG_WARN ("OneToManyProcessor closeAll");
    std::map<std::string, MediaSink*>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      (*it).second->closeSink();
    }
    this->publisher->closeSource();
    ELOG_DEBUG("CloseSource Done");
  }

}/* namespace erizo */

