/*
 * RtpSink.h
 *
 *  Created on: Aug 2, 2012
 *      Author: pedro
 */

#ifndef RTPSINK_H_
#define RTPSINK_H_

#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <queue>

#include "../MediaDefinitions.h"
#include "../logger.h"

namespace erizo {

class RtpSink: public MediaSink {
  DECLARE_LOGGER();
  public:
	RtpSink(const std::string& url, const std::string& port);
	virtual ~RtpSink();

private:

  boost::scoped_ptr<boost::asio::ip::udp::socket> socket_;
  boost::scoped_ptr<boost::asio::ip::udp::resolver> resolver_;

  boost::scoped_ptr<boost::asio::ip::udp::resolver::query> query_;
  boost::scoped_ptr<boost::asio::io_service> ioservice_;
	boost::asio::ip::udp::resolver::iterator iterator_;

  boost::thread send_Thread_;
	boost::condition_variable cond_;
  boost::mutex queueMutex_;
  std::queue<dataPacket> sendQueue_;
  bool sending_;

  int deliverAudioData_(char* buf, int len);
  int deliverVideoData_(char* buf, int len);
	int sendData(char* buffer, int len);
  void sendLoop();
	void queueData(const char* buffer, int len, packetType type);
};

} /* namespace erizo */
#endif /* RTPSINK_H_ */
