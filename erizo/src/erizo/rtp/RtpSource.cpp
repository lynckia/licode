
/*
 * RtpSource.cpp
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#include "RtpSource.h"
using boost::asio::ip::udp;

namespace erizo {
DEFINE_LOGGER(RtpSource, "RtpSource");

RtpSource::RtpSource(const int port) {
  socket_.reset(new boost::asio::ip::udp::socket(io_service_, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)));
  running_=true;
  rtpSource_thread_= boost::thread(&RtpSource::getDataLoop,this);
}

RtpSource::~RtpSource() {
  running_ = false;
  rtpSource_thread_.join();

}

void RtpSource::getDataLoop() {
  size_t length;
  while(running_){
    boost::asio::ip::udp::endpoint sender_endpoint;    
    length = socket_->receive_from(boost::asio::buffer(buffer_, LENGTH), sender_endpoint);
    if (length>0&&this->videoSink_){
      this->videoSink_->deliverVideoData((char*)buffer_, (int)length);
    }
  }
}

} /* namespace erizo */
