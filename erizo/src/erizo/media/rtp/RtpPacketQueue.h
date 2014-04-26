#ifndef __RTPPACKETQUEUE_H__
#define __RTPPACKETQUEUE_H__

#include <map>
#include <boost/shared_ptr.hpp>

#include "logger.h"

namespace erizo{
  //forward declaration
  class dataPacket; 
  
  class RtpPacketQueue
  {
    DECLARE_LOGGER();

    public:
    RtpPacketQueue();
    virtual ~RtpPacketQueue(void);

    void  pushPacket(const char *data, int length);
    boost::shared_ptr<dataPacket> popPacket();
    int getSize();

    private:
    static const int MAX_DIFF = 50;
    static const int MAX_DIFF_TS = 50000;
    static const unsigned int MAX_SIZE = 10;
    std::map< int, boost::shared_ptr<dataPacket>> queue_;
    uint16_t lastNseq_;
    uint32_t lastTs_;

    void enqueuePacket(const char *data, int length, uint16_t nseq);
    void cleanQueue(void);

  };
} /* namespace erizo */

#endif /* RTPPACKETQUEUE*/

