/*
 * RtpSink.cpp
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#include "RtpSink.h"
using boost::asio::ip::udp;

namespace erizo {
  DEFINE_LOGGER(RtpSink, "RtpSink");

  RtpSink::RtpSink(const std::string& url, const std::string& port) {
    ioservice_.reset(new boost::asio::io_service);
    resolver_.reset(new udp::resolver(*ioservice_));
    socket_.reset(new udp::socket(*ioservice_, udp::endpoint(udp::v4(), 40000)));
    query_.reset(new udp::resolver::query(udp::v4(), url.c_str(), port.c_str()));
    iterator_ = resolver_->resolve(*query_);
    sending_ =true;
    send_Thread_ = boost::thread(&RtpSink::sendLoop, this);
  }

  RtpSink::~RtpSink() {

  }

  int RtpSink::deliverVideoData_(char* buf, int len){
    this->queueData(buf, len, VIDEO_PACKET);
    return 0;
  }

  int RtpSink::deliverAudioData_(char* buf, int len){
    this->queueData(buf, len, AUDIO_PACKET);
    return 0;
  }

  int RtpSink::sendData(char* buffer, int len) {
    socket_->send_to(boost::asio::buffer(buffer, len), *iterator_);
    return len;
  }

	void RtpSink::queueData(const char* buffer, int len, packetType type){
    boost::mutex::scoped_lock lock(queueMutex_);
    if (sending_==false)
      return;
    if (sendQueue_.size() < 1000) {
      dataPacket p_;
      memcpy(p_.data, buffer, len);
      p_.type = type;
      p_.length = len;
      sendQueue_.push(p_);
    }
    cond_.notify_one();
  }

  void RtpSink::sendLoop(){
    while (sending_ == true) {

      boost::unique_lock<boost::mutex> lock(queueMutex_);
      while (sendQueue_.size() == 0) {
        cond_.wait(lock);
        if (sending_ == false) {
          lock.unlock();
          return;
        }
      }
      if(sendQueue_.front().comp ==-1){
        sending_ =  false;
        ELOG_DEBUG("Finishing send Thread, packet -1");
        sendQueue_.pop();
        lock.unlock();
        return;
      }
      this->sendData(sendQueue_.front().data, sendQueue_.front().length);
      sendQueue_.pop();
      lock.unlock();
    }
  }

} /* namespace erizo */
