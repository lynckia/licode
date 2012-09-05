/*
 * RTPSink.h
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#ifndef RTPSINK_H_
#define RTPSINK_H_

#include <boost/asio.hpp>


namespace erizo {

class RTPSink {
public:
	RTPSink(const std::string& url, const std::string& port);
	int sendData(unsigned char* buffer, int len);
	virtual ~RTPSink();

private:

	boost::asio::ip::udp::socket* socket_;
	boost::asio::ip::udp::resolver* resolver_;

	boost::asio::ip::udp::resolver::query* query_;
	boost::asio::io_service* ioservice_;
	boost::asio::ip::udp::resolver::iterator iterator_;
};


} /* namespace erizo */
#endif /* RTPSINK_H_ */
