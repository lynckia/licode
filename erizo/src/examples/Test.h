#include <media/MediaProcessor.h>
#include <media/utils/RtpUtils.h>
#include <boost/asio.hpp>

#ifndef TEST_H_
#define TEST_H_
namespace erizo {
class Test: public RawDataReceiver {
public:
	Test();
	virtual ~Test();
	void receiveRawData(unsigned char*data, int len);

	void rec();
	void send(char *buff, int buffSize);
private:

	boost::asio::ip::udp::socket* socket_;
	boost::asio::ip::udp::resolver* resolver_;

	boost::asio::ip::udp::resolver::query* query_;
	boost::asio::io_service* ioservice_;
	InputProcessor* ip;
	erizo::RtpParser pars;

};

}
#endif /* TEST_H_ */
