#include "rtp/RtpPacketQueue.h"

#include <cstring>

#include "./MediaDefinitions.h"
#include "rtp/RtpHeaders.h"

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(RtpPacketQueue, "rtp.RtpPacketQueue");

RtpPacketQueue::RtpPacketQueue(double depthInSeconds, double maxDepthInSeconds) :
  lastSequenceNumberGiven_(-1), timebase_(0), depthInSeconds_(depthInSeconds), maxDepthInSeconds_(maxDepthInSeconds) {
  if (depthInSeconds_ >= maxDepthInSeconds_) {
      ELOG_WARN("invalid configuration, depth_: %f, max_: %f; reset to defaults",
                 depthInSeconds_, maxDepthInSeconds_);
      depthInSeconds_ = erizo::DEFAULT_DEPTH;
      maxDepthInSeconds_ = erizo::DEFAULT_MAX;
  }
}

RtpPacketQueue::~RtpPacketQueue(void) {
  queue_.clear();
}

void RtpPacketQueue::pushPacket(const char *data, int length) {
  const RtpHeader *currentHeader = reinterpret_cast<const RtpHeader*>(data);
  uint16_t currentSequenceNumber = currentHeader->getSeqNumber();

  if (lastSequenceNumberGiven_ >= 0 &&
        (rtpSequenceLessThan(currentSequenceNumber, (uint16_t)lastSequenceNumberGiven_) ||
        currentSequenceNumber == lastSequenceNumberGiven_)) {
    // this sequence number is less than the stuff we've already handed out,
    // which means it's too late to be of any value.
    ELOG_WARN("SSRC:%u, Payload: %u, discarding very late sample %d that is <= %d",
              currentHeader->getSSRC(),
              currentHeader->getPayloadType(),
              currentSequenceNumber,
              lastSequenceNumberGiven_);
    return;
  }

  // TODO(pedro) this should be a secret of the DataPacket class.  It should maintain its own memory
  // and copy stuff as necessary.
  boost::shared_ptr<DataPacket> packet(new DataPacket());
  memcpy(packet->data, data, length);
  packet->length = length;

  // let's insert this packet where it belongs in the queue.
  boost::mutex::scoped_lock lock(queueMutex_);
  std::list<boost::shared_ptr<DataPacket> >::iterator it;
  for (it=queue_.begin(); it != queue_.end(); ++it) {
    const RtpHeader *header = reinterpret_cast<const RtpHeader*>((*it)->data);
    uint16_t sequenceNumber = header->getSeqNumber();

    if (sequenceNumber == currentSequenceNumber) {
      // We already have this sequence number in the queue.
      ELOG_INFO("discarding duplicate sample %d", currentSequenceNumber);
      break;
    }

    if (this->rtpSequenceLessThan(sequenceNumber, currentSequenceNumber)) {
      queue_.insert(it, packet);
      break;
    }
  }

  if (it == queue_.end()) {
    // something old, or queue is empty.
    queue_.push_back(packet);
  }

  // Enforce our max queue size.
  while (getDepthInSeconds() > maxDepthInSeconds_) {
    ELOG_WARN("RtpPacketQueue - Discarding a sample due to excessive queue depth");
    queue_.pop_back();  // remove oldest samples.
  }
}

// pops a packet off the queue, respecting the specified queue depth.
boost::shared_ptr<DataPacket> RtpPacketQueue::popPacket(bool ignore_depth) {
  boost::shared_ptr<DataPacket> packet;

  boost::mutex::scoped_lock lock(queueMutex_);
  if (queue_.size() > 0) {
    if (ignore_depth || getDepthInSeconds() > depthInSeconds_) {
      packet = queue_.back();
      queue_.pop_back();
      const RtpHeader *header = reinterpret_cast<const RtpHeader*>(packet->data);
      lastSequenceNumberGiven_ = static_cast<int>(header->getSeqNumber());
    }
  }

  return packet;
}

void RtpPacketQueue::setTimebase(unsigned int timebase) {
  boost::mutex::scoped_lock lock(queueMutex_);
  timebase_ = timebase;
}

int RtpPacketQueue::getSize() {
  boost::mutex::scoped_lock lock(queueMutex_);
  return queue_.size();
}

double RtpPacketQueue::getDepthInSeconds() {
  // must be called while queueMutex_ is taken.  Private method.  Also, if no timebase has been set, this always
  // returns zero because we have no way of interpreting how much data is in the queue.
  double depth = 0.0;
  if (timebase_ > 0 && queue_.size() > 1) {
    const RtpHeader *oldest = reinterpret_cast<const RtpHeader*>(queue_.back()->data);
    const RtpHeader *newest = reinterpret_cast<const RtpHeader*>(queue_.front()->data);
    depth = (static_cast<double>(newest->getTimestamp() - oldest->getTimestamp())) / static_cast<double>(timebase_);
  }

  return depth;
}

bool RtpPacketQueue::hasData() {
  boost::mutex::scoped_lock lock(queueMutex_);
  double currentDepth = getDepthInSeconds();
  return currentDepth > depthInSeconds_;
}

// Implements x < y, taking into account RTP sequence number wrap
// The general idea is if there's a very large difference between
// x and y, that implies that the larger one is actually "less than"
// the smaller one.
//
// I picked 0x8000 as my "very large" threshold because it splits
// 0xffff, so it seems like a logical choice.
bool RtpPacketQueue::rtpSequenceLessThan(uint16_t x, uint16_t y) {
  int diff = y - x;
  if (diff > 0) {
    return (diff < 0x8000);
  } else if (diff < 0) {
    return (diff < -0x8000);
  } else {  // diff == 0
    return false;
  }
}
}  // namespace erizo
