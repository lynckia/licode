
/*
 * RtpSource.h
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#ifndef RTPSOURCE_H_
#define RTPSOURCE_H_

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "../MediaDefinitions.h"
#include "../logger.h"

namespace erizo {

class RtpSource: public MediaSource {
	DECLARE_LOGGER();

public:
  RtpSource(const int port);  
	virtual ~RtpSource();

private:

  static const int LENGTH = 1500;
  boost::scoped_ptr<boost::asio::ip::udp::socket> socket_;
	boost::asio::io_service io_service_;
  boost::thread rtpSource_thread_;
  char* buffer_[LENGTH];
  bool running_;
	void getDataLoop();
};


} /* namespace erizo */
#endif /* RTPSINK_H_ */
