/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"
#include "WebRtcConnection.h"

namespace erizo {
  OneToManyProcessor::OneToManyProcessor() {

      sendVideoBuffer_ = (char*) malloc(20000);
      sendAudioBuffer_ = (char*) malloc(20000);
      publisher = NULL;
      feedbackSink_ = NULL;
      sentPackets_ = 0;

    }

  OneToManyProcessor::~OneToManyProcessor() {
    this->closeAll();
    if (sendVideoBuffer_)
      delete sendVideoBuffer_;
    if (sendAudioBuffer_)
      delete sendAudioBuffer_;
  }

  int OneToManyProcessor::deliverAudioData(char* buf, int len) {
    if (subscribers.empty() || len <= 0)
      return 0;

    std::map<std::string, MediaSink*>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      //memset(sendAudioBuffer_, 0, len);
      //memcpy(sendAudioBuffer_, buf, len);
      (*it).second->deliverAudioData(buf, len);
    }

    return 0;
  }

  int OneToManyProcessor::deliverVideoData(char* buf, int len) {
    if (subscribers.empty() || len <= 0)
      return 0;

    rtcpheader* head = reinterpret_cast<rtcpheader*>(buf);
    if(head->packettype==RTCP_Receiver_PT || head->packettype==RTCP_Feedback_PT){
      printf("recibo feedback por donde no es %d\n", head->packettype);
      if (feedbackSink_){
        head->ssrc = htonl(publisher->getVideoSourceSSRC());
        feedbackSink_->deliverFeedback(buf,len);
      }
      return 0;
    }
    std::map<std::string, MediaSink*>::iterator it;
    //printf("Sending video data to subscribers of %u\n", publisher->getVideoSourceSSRC());
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      //memset(sendVideoBuffer_, 0, len);
      //memcpy(sendVideoBuffer_, buf, len);
      //printf(" Subscriber %u\n", (*it).second->getVideoSinkSSRC());
      (*it).second->deliverVideoData(buf, len);
    }
    sentPackets_++;
    return 0;
  }

  void OneToManyProcessor::setPublisher(MediaSource* webRtcConn) {
    printf("SET PUBLISHER\n");
    this->publisher = webRtcConn;
    feedbackSink_ = publisher->getFeedbackSink();
  }

  int OneToManyProcessor::deliverFeedback(char* buf, int len){
    feedbackSink_->deliverFeedback(buf,len);
    return 0;

  }

  void OneToManyProcessor::addSubscriber(MediaSink* webRtcConn,
      const std::string& peerId) {
    printf("Adding subscriber\n");
    printf("From %u, %u \n", publisher->getAudioSourceSSRC() , publisher->getVideoSourceSSRC());
    webRtcConn->setAudioSinkSSRC(this->publisher->getAudioSourceSSRC());
    webRtcConn->setVideoSinkSSRC(this->publisher->getVideoSourceSSRC());
    printf("Subscribers ssrcs: Audio %u, video, %u from %u, %u \n", webRtcConn->getAudioSinkSSRC(), webRtcConn->getVideoSinkSSRC(), this->publisher->getAudioSourceSSRC() , this->publisher->getVideoSourceSSRC());
    FeedbackSource* fbsource = webRtcConn->getFeedbackSource();

    if (fbsource!=NULL){
      printf("adding fbsource************************************************\n\n\n");
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
    this->close();
  }

  void OneToManyProcessor::close(){
    this->closeAll();
  }

  void OneToManyProcessor::closeAll() {
    std::map<std::string, MediaSink*>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); it++) {
      (*it).second->closeSink();
    }
    this->publisher->closeSource();
  }

}/* namespace erizo */

