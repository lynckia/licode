/*
 * RtpSink.cpp
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#include "rtp/RtpSink.h"

#include <string>
#include <cstring>

using std::memcpy;
using boost::asio::ip::udp;

namespace erizo {
  DEFINE_LOGGER(RtpSink, "rtp.RtpSink");

  RtpSink::RtpSink(const std::string& url, const std::string& port, int feedbackPort) {
    resolver_.reset(new udp::resolver(io_service_));
    socket_.reset(new udp::socket(io_service_, udp::endpoint(udp::v4(), 0)));
    fbSocket_.reset(new udp::socket(io_service_, udp::endpoint(udp::v4(), feedbackPort)));
    query_.reset(new udp::resolver::query(udp::v4(), url.c_str(), port.c_str()));
    iterator_ = resolver_->resolve(*query_);
    sending_ = true;
    boost::asio::ip::udp::endpoint sender_endpoint;
    fbSocket_->async_receive_from(boost::asio::buffer(buffer_, LENGTH), sender_endpoint,
        boost::bind(&RtpSink::handleReceive, this, boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
    send_Thread_ = boost::thread(&RtpSink::sendLoop, this);
    receive_Thread_ = boost::thread(&RtpSink::serviceLoop, this);
  }

  RtpSink::~RtpSink() {
    sending_ = false;
    send_Thread_.join();
    io_service_.stop();
    receive_Thread_.join();
  }

  int RtpSink::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
    this->queueData(video_packet->data, video_packet->length, VIDEO_PACKET);
    return 0;
  }

  int RtpSink::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
    this->queueData(audio_packet->data, audio_packet->length, AUDIO_PACKET);
    return 0;
  }

  int RtpSink::sendData(char* buffer, int len) {
    socket_->send_to(boost::asio::buffer(buffer, len), *iterator_);
    return len;
  }

  void RtpSink::queueData(const char* buffer, int len, packetType type) {
    boost::mutex::scoped_lock lock(queueMutex_);
    if (sending_ == false)
      return;
    if (sendQueue_.size() < 1000) {
      DataPacket p_;
      memcpy(p_.data, buffer, len);
      p_.type = type;
      p_.length = len;
      sendQueue_.push(p_);
    }
    cond_.notify_one();
  }

  void RtpSink::sendLoop() {
    while (sending_) {
      boost::unique_lock<boost::mutex> lock(queueMutex_);
      while (sendQueue_.size() == 0) {
        cond_.wait(lock);
        if (!sending_) {
          return;
        }
      }
      if (sendQueue_.front().comp ==-1) {
        sending_ =  false;
        ELOG_DEBUG("Finishing send Thread, packet -1");
        sendQueue_.pop();
        return;
      }
      this->sendData(sendQueue_.front().data, sendQueue_.front().length);
      sendQueue_.pop();
    }
  }

  void RtpSink::handleReceive(const::boost::system::error_code& error, size_t bytes_recvd) {  // NOLINT
    auto fb_sink = fb_sink_.lock();
    if (bytes_recvd > 0 && fb_sink) {
      fb_sink->deliverFeedback(std::make_shared<DataPacket>(0, reinterpret_cast<char*>(buffer_),
            static_cast<int>(bytes_recvd), OTHER_PACKET));
    }
  }

  void RtpSink::serviceLoop() {
    io_service_.run();
  }

} /* namespace erizo */
