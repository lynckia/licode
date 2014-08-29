#ifndef __RTPPACKETQUEUE_H__
#define __RTPPACKETQUEUE_H__

#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include "logger.h"

namespace erizo{
//forward declaration
class dataPacket;

static const unsigned int DEFAULT_MAX = 120;
static const unsigned int DEFAULT_DEPTH = 30;

// This class implements a packet reordering queue. Here's what it does:
//
// 1. Receives incoming packets and pushes them into a queue based on sequence number
// 2. Rejects duplicate packets--duplicate sequence numbers are dropped on the floor
// 3. Handles sequence number wrap (e.g. packet "1" is technically greater than "65535" because that
//    is a sequence number wrap
// 4. Handles out of order packets
// 5. Handles late packets.  Packets with sequence number greater than the last sequence number
//    handed out through popPacket are discarded.  There's a log message to help identify
//    a sane value for their queue depth.
// 6. Is threadsafe.  All public methods lock to ensure the container isn't fouled by multithreaded
//    access.  This also prevents a minimal amount of locking in calling classes, which prevents
//    blocking of worker threads.
//
// Usage is pretty straight-forward:
//
// 1. instantiate and push incoming data with pushPacket().
// 2. check if data is ready to be popped by calling hasData().
// 3. pop data with popPacket().  popPacket returns a null shared_ptr if nothing is available.
// 4. popPacket() can be called with an override to drain the queue regardless of the current depth.
class RtpPacketQueue
{
    DECLARE_LOGGER();

public:
    RtpPacketQueue(unsigned int max = DEFAULT_MAX, unsigned int depth = DEFAULT_DEPTH );
    virtual ~RtpPacketQueue(void);
    void  pushPacket(const char *data, int length);
    boost::shared_ptr<dataPacket> popPacket(bool ignore_depth = false);
    int getSize();  // total size of all items in the queue
    bool hasData(); // whether or not current queue depth is >= depth_

private:
    boost::mutex queueMutex_;
    std::list<boost::shared_ptr<dataPacket> > queue_;
    int lastSequenceNumberGiven_;
    bool rtpSequenceLessThan(uint16_t x, uint16_t y);
    unsigned int max_;       // The max size we allow the queue to grow before discarding samples
    unsigned int depth_;     // The depth at which pop packet functions.
};
} /* namespace erizo */

#endif /* RTPPACKETQUEUE*/

