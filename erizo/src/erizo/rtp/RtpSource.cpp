
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
      const std::string& feedbackPort):feedbackDir_(feedbackDir), feedbackPort_(feedbackPort){
    sequenceNumberFIR_ = 0;
    ELOG_DEBUG("Starting RTPSOURCE. MediaPort %d, feedbackIP %s, feedBackPort %s", mediaPort, feedbackDir.c_str(), 
        feedbackPort.c_str());
    sourcefbSink_ = this;
    socket_.reset(new udp::socket(io_service_, 
          udp::endpoint(udp::v4(), 
            mediaPort)));
    resolver_.reset(new udp::resolver(io_service_));
    fbSocket_.reset(new udp::socket(io_service_sync_, udp::endpoint(udp::v4(), 60000)));
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

  int RtpSource::sendFirPacket(){/*
    sequenceNumberFIR_++; // do not increase if repetition
    int pos = 0;
    uint8_t rtcpPacket[50];
    // add full intra request indicator
    uint8_t FMT = 4;
    rtcpPacket[pos++] = (uint8_t) 0x80 + FMT;
    rtcpPacket[pos++] = (uint8_t) 206;

    //Length of 4
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) (4);

    // Add our own SSRC
    uint32_t* ptr = reinterpret_cast<uint32_t*>(rtcpPacket + pos);
    ptr[0] = htonl(this->getVideoSinkSSRC());
    pos += 4;

    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;
    // Additional Feedback Control Information (FCI)
    uint32_t* ptr2 = reinterpret_cast<uint32_t*>(rtcpPacket + pos);
    ptr2[0] = htonl(this->getVideoSourceSSRC());
    pos += 4;

    rtcpPacket[pos++] = (uint8_t) (sequenceNumberFIR_);
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;
    rtcpPacket[pos++] = (uint8_t) 0;

    if (videoTransport_ != NULL) {

      boost::unique_lock<boost::mutex> lock(receiveVideoMutex_);
      videoTransport_->write((char*)rtcpPacket, pos);
    }
    */
    return 0;
  }

  int RtpSource::deliverFeedback_(char* buf, int len){
    if(fbSocket_.get()!=NULL){
//      ELOG_DEBUG("Sending feedback");
      
      try{
        boost::asio::ip::udp::endpoint destination(
        boost::asio::ip::address::from_string(feedbackDir_),50001);
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
        if (videoSink_){
//          ELOG_DEBUG("Deliver Video %d", bytes_recvd);
          this->videoSink_->deliverVideoData((char*)buffer_, (int)bytes_recvd);
        }
      }else if (head->payloadtype == PCMU_8000_PT || head->payloadtype == OPUS_48000_PT){
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
