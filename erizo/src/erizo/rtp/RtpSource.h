
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

class RtpSource: public MediaSource, public FeedbackSink {
	DECLARE_LOGGER();

public:
  RtpSource(const int mediaPort, const std::string& feedbackDir, 
      const int feedbackPort);
  void setFeedbackInfo(const std::string& feedbackDir, const int feedbackPort);
  unsigned int getMediaPort();
  int sendFirPacket();
	virtual ~RtpSource();

private:
  unsigned int mediaPort_, sequenceNumberFIR_;
  std::string feedbackDir_;
  int feedbackPort_;
  static const int LENGTH = 1500;
  boost::scoped_ptr<boost::asio::ip::udp::socket> socket_, fbSocket_;
  boost::asio::ip::udp::endpoint sender_endpoint_, receive_endpoint_;
	boost::asio::io_service io_service_;
	boost::asio::io_service io_service_sync_;
  boost::thread rtpSource_thread_;
  char* buffer_[LENGTH];
  bool running_;
  void handleReceive(const::boost::system::error_code& error, 
    size_t bytes_recvd);
  void eventLoop();
  int deliverFeedback_(char* buf, int len);
};


} /* namespace erizo */
#endif /* RTPSOURCE_H_ */
