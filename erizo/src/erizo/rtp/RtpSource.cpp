
/*
 * RtpSource.cpp
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#include "RtpSource.h"
#include "RtpHeaders.h"
using boost::asio::ip::udp;

namespace erizo {
  DEFINE_LOGGER(RtpSource, "RtpSource");

  RtpSource::RtpSource(const int mediaPort, const std::string& feedbackDir, 
      const int feedbackPort):mediaPort_(mediaPort),feedbackDir_(feedbackDir), feedbackPort_(feedbackPort){
    sequenceNumberFIR_ = 0;
    ELOG_DEBUG("Starting RTPSOURCE. MediaPort %d, feedbackIP %s, feedBackPort %u", mediaPort, feedbackDir.c_str(), 
        feedbackPort);
    sourcefbSink_ = this;
    fbSocket_.reset(new udp::socket(io_service_sync_, udp::endpoint(udp::v4(), 0)));
    receive_endpoint_ = boost::asio::ip::udp::endpoint(udp::v4(),(unsigned short)mediaPort_);
    ELOG_DEBUG("The mediaPort is %u", mediaPort_);
    videoSourceSSRC_ = 0;
    audioSourceSSRC_ = 0;
    socket_.reset(new udp::socket(io_service_, 
          receive_endpoint_));
    socket_->async_receive_from(boost::asio::buffer(buffer_, LENGTH), sender_endpoint_, 
        boost::bind(&RtpSource::handleReceive, this, boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
    rtpSource_thread_= boost::thread(&RtpSource::eventLoop,this);
   
  }

  RtpSource::~RtpSource() {
    ELOG_DEBUG("RTPSOURCE_DESTRUCTOR");
    io_service_.stop();
    rtpSource_thread_.join();

  }
  unsigned int RtpSource::getMediaPort(){
    return socket_->local_endpoint().port();
  }

  void RtpSource::setFeedbackInfo(const std::string& feedbackDir, const int feedbackPort){
    /*
    feedbackDir_ = feedbackDir;
    feedbackPort_ = feedbackPort; 
    fbSocket_.reset(new udp::socket(io_service_sync_, udp::endpoint(udp::v4(), 0)));
    */
  }

  int RtpSource::sendFirPacket(){
    return 0;
  }

  int RtpSource::deliverFeedback_(char* buf, int len){
    if(fbSocket_.get()!=NULL){
//      ELOG_DEBUG("Sending feedback");
      
      try{
        boost::asio::ip::udp::endpoint destination(
        boost::asio::ip::address::from_string(feedbackDir_),feedbackPort_);
        fbSocket_->send_to(boost::asio::buffer(buf, len),destination);
      } catch(std::exception& e){        
        ELOG_ERROR("Error %s", e.what());
      }
     
    }
    return len;

  }

  void RtpSource::handleReceive(const::boost::system::error_code& error, 
      size_t bytes_recvd) {
    if (bytes_recvd>0){
      RtpHeader *head = reinterpret_cast<RtpHeader*> (buffer_);
      if (head->payloadtype ==VP8_90000_PT || head->payloadtype == RED_90000_PT){ 
          if (!videoSourceSSRC_){
            videoSourceSSRC_ = head->getSSRC();
            ELOG_DEBUG("VideoSourceSSRC set to %u", videoSourceSSRC_);
          }
        if (videoSink_){
//          ELOG_DEBUG("Deliver Video %d", bytes_recvd);
          this->videoSink_->deliverVideoData((char*)buffer_, (int)bytes_recvd);
        }
      }else if (head->payloadtype == PCMU_8000_PT || head->payloadtype == OPUS_48000_PT){
          if (!audioSourceSSRC_){
            audioSourceSSRC_ = head->getSSRC();
            ELOG_DEBUG("AudioSourceSSRC set to %u", audioSourceSSRC_);
          }
        if (audioSink_){
//          ELOG_DEBUG("Deliver Audio %d", bytes_recvd);
          this->audioSink_->deliverAudioData((char*)buffer_, (int)bytes_recvd);
        }
      }
    }
    socket_->async_receive_from(boost::asio::buffer(buffer_, LENGTH), sender_endpoint_, 
        boost::bind(&RtpSource::handleReceive, this, boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }
  void RtpSource::eventLoop() {
    ELOG_DEBUG("io_service run");
    io_service_.run();
    ELOG_DEBUG("io_service finish run");
  }

} /* namespace erizo */
