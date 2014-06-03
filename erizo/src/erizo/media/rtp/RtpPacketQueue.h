#ifndef __RTPPACKETQUEUE_H__
#define __RTPPACKETQUEUE_H__

#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
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
    boost::mutex queueMutex_;
    static const unsigned int MAX_SIZE = 120;
    std::list<boost::shared_ptr<dataPacket> > queue_;
    int lastSequenceNumberGiven_;
    bool rtpSequenceLessThan(uint16_t x, uint16_t y);
};
} /* namespace erizo */

#endif /* RTPPACKETQUEUE*/

