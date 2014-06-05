/*
 * RtpSink.h
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#ifndef RTPSINK_H_
#define RTPSINK_H_

#include <boost/asio.hpp>

#include "../MediaDefinitions.h"

namespace erizo {

class RtpSink: public MediaSink {
public:
	RtpSink(const std::string& url, const std::string& port);
	int sendData(unsigned char* buffer, int len);
	virtual ~RtpSink();

private:

  boost::scoped_ptr<boost::asio::ip::udp::socket> socket_;
  boost::scoped_ptr<boost::asio::ip::udp::resolver> resolver_;

  boost::scoped_ptr<boost::asio::ip::udp::resolver::query> query_;
  boost::scoped_ptr<boost::asio::io_service> ioservice_;
	boost::asio::ip::udp::resolver::iterator iterator_;

  int deliverAudioData_(char* buf, int len);
  int deliverVideoData_(char* buf, int len);
};


} /* namespace erizo */
#endif /* RTPSINK_H_ */
