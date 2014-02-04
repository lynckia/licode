/*
 * RTPSink.cpp
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#include "RTPSink.h"
using boost::asio::ip::udp;

namespace erizo {

RTPSink::RTPSink(const std::string& url, const std::string& port) {
	ioservice_ = new boost::asio::io_service;
	resolver_ = new udp::resolver(*ioservice_);
	socket_ = new udp::socket(*ioservice_, udp::endpoint(udp::v4(), 40000));
	query_ = new udp::resolver::query(udp::v4(), url.c_str(), port.c_str());
	iterator_ = resolver_->resolve(*query_);
}

RTPSink::~RTPSink() {

	delete ioservice_;
	delete resolver_;
	delete socket_;
	delete query_;

}

int RTPSink::sendData(unsigned char* buffer, int len) {
	socket_->send_to(boost::asio::buffer(buffer, len), *iterator_);

	return len;
}

} /* namespace erizo */
