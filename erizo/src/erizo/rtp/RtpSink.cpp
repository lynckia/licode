/*
 * RtpSink.cpp
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#include "RtpSink.h"
using boost::asio::ip::udp;

namespace erizo {

RtpSink::RtpSink(const std::string& url, const std::string& port) {
	ioservice_.reset(new boost::asio::io_service);
	resolver_.reset(new udp::resolver(*ioservice_));
	socket_.reset(new udp::socket(*ioservice_, udp::endpoint(udp::v4(), 40000)));
	query_.reset(new udp::resolver::query(udp::v4(), url.c_str(), port.c_str()));
	iterator_ = resolver_->resolve(*query_);
}

RtpSink::~RtpSink() {

}

int RtpSink::deliverVideoData_(char* buf, int len){
  return 0;
}

int RtpSink::deliverAudioData_(char* buf, int len){
  return 0;
}

int RtpSink::sendData(unsigned char* buffer, int len) {
	socket_->send_to(boost::asio::buffer(buffer, len), *iterator_);

	return len;
}

} /* namespace erizo */
