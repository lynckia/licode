#include <cstring>

#include "RtpPacketQueue.h"
#include "../../MediaDefinitions.h"
#include "RtpHeader.h"


namespace erizo{

  DEFINE_LOGGER(RtpPacketQueue, "RtpPacketQueue");

  RtpPacketQueue::RtpPacketQueue()
    : lastNseq_(0), lastTs_(0)
  {
  }

  RtpPacketQueue::~RtpPacketQueue(void)
  {
    cleanQueue();
  }

  void RtpPacketQueue::pushPacket(const char *data, int length)
  {

    const RTPHeader *header = reinterpret_cast<const RTPHeader*>(data);
    uint16_t nseq = header->getSeqNumber();
    uint32_t ts = header->getTimestamp();

    long long int ltsdiff = (long long int)ts - (long long int)lastTs_;
    int tsdiff = (int)ltsdiff;
    int nseqdiff = nseq - lastNseq_;
    /*
    // nseq sequence cicle test
    if ( abs(nseqdiff) > ( USHRT_MAX - MAX_DIFF ) )
    {
    NOTIFY("Vuelta del NSeq ns=%d last=%d\n", nseq, lastNseq_);
    if (nseqdiff > 0)
    nseqdiff-= (USHRT_MAX + 1);
    else
    nseqdiff+= (USHRT_MAX + 1);
    }
    */

    if (abs(tsdiff) > MAX_DIFF_TS || abs(nseqdiff) > MAX_DIFF )
    {
      // new flow, process and clean queue
      ELOG_DEBUG("Max diff reached, new Flow? nsqediff %d , tsdiff %d", nseqdiff, tsdiff);
      ELOG_DEBUG("PT %d", header->getPayloadType());
      lastNseq_ = nseq;
      lastTs_ = ts;
      cleanQueue();
      enqueuePacket(data, length, nseq);
    }
    else if (nseqdiff > 1)
    {
      // Jump in nseq, enqueue
      ELOG_DEBUG("Jump in nseq");
      enqueuePacket(data, length, nseq);
    }
    else if (nseqdiff == 1)
    {
      // next packet, process
      lastNseq_ = nseq;
      lastTs_ = ts;
      enqueuePacket(data, length, nseq);
    }
    else if (nseqdiff < 0)
    {
      ELOG_DEBUG("Old Packet Received");
      // old packet, discard?
      // stats?
    }
    else if (nseqdiff == 0)
    {
      ELOG_DEBUG("Duplicate Packet received");
      //duplicate packet, process (for stats)?
    }
  }

  void RtpPacketQueue::enqueuePacket(const char *data, int length, uint16_t nseq)
  {
    if (queue_.size() > MAX_SIZE) { // if queue is growing too much, we start again
      cleanQueue();
    }

    boost::shared_ptr<dataPacket> packet(new dataPacket());
    memcpy(packet->data, data, length);
    packet->length = length;
    queue_.insert(std::map< int, boost::shared_ptr<dataPacket>>::value_type(nseq,packet));

  }

  void RtpPacketQueue::cleanQueue()
  {
    queue_.clear();
  }

  boost::shared_ptr<dataPacket> RtpPacketQueue::popPacket()
  {
    boost::shared_ptr<dataPacket> packet = queue_.begin()->second;
    if (packet.get() == NULL){
      return packet;
    }
    const RTPHeader *header = reinterpret_cast<const RTPHeader*>(packet->data);
    lastNseq_ = queue_.begin()->first;
    lastTs_ = header->getTimestamp();
    queue_.erase(queue_.begin());
    return packet;
  }

  int RtpPacketQueue::getSize(){
    uint16_t size = queue_.size();
    return size;      
  }
} /* namespace erizo */
