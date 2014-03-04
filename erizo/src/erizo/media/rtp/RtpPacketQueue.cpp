#include "RtpPacketQueue.h"
#include "RtpHeader.h"

#include <cstring>

namespace erizo{

  // -----------------------------------------------------------------------------
  // RtpPacketQueue::RtpPacketQueue
  //
  // -----------------------------------------------------------------------------
  //
  RtpPacketQueue::RtpPacketQueue()
    : lastNseq(0), lastTs(0)
  {
  }


  // -----------------------------------------------------------------------------
  // RtpPacketQueue::~RtpPacketQueue
  //
  // -----------------------------------------------------------------------------
  //
  RtpPacketQueue::~RtpPacketQueue(void)
  {
    cleanQueue();
  }

  // -----------------------------------------------------------------------------
  // RtpPacketQueue::packetReceived
  //
  // -----------------------------------------------------------------------------
  //
  void RtpPacketQueue::packetReceived(const unsigned char *data, int length)
  {
    //channel->packetReceived2(data, length);
    //return;

    const RTPHeader *header = reinterpret_cast<const RTPHeader*>(data);
    uint16_t nseq = header->getSeqNumber();
    uint32_t ts = header->getTimestamp();

    long long int ltsdiff = (long long int)ts - (long long int)lastTs;
    int tsdiff = (int)ltsdiff;
    int nseqdiff = nseq - lastNseq;
    /*
    // nseq sequence cicle test
    if ( abs(nseqdiff) > ( USHRT_MAX - MAX_DIFF ) )
    {
    NOTIFY("Vuelta del NSeq ns=%d last=%d\n", nseq, lastNseq);
    if (nseqdiff > 0)
    nseqdiff-= (USHRT_MAX + 1);
    else
    nseqdiff+= (USHRT_MAX + 1);
    }
    */
    if (abs(tsdiff) > MAX_DIFF_TS || abs(nseqdiff) > MAX_DIFF )
    {
      // new flow, process and clean queue
      //channel->packetReceived2(data, length);
      lastNseq = nseq;
      lastTs = ts;
      cleanQueue();
    }
    else if (nseqdiff > 1)
    {
      // Jump in nseq, enqueue
      enqueuePacket(data, length, nseq);
      checkQueue();
    }
    else if (nseqdiff == 1)
    {
      // next packet, process
      // channel->packetReceived2(data, length);
      lastNseq = nseq;
      lastTs = ts;
      checkQueue();
    }
    else if (nseqdiff < 0)
    {
      // old packet, discard?
      // stats?
    }
    else if (nseqdiff == 0)
    {
      //duplicate packet, process (for stats)?
    }
  }

  // -----------------------------------------------------------------------------
  // RtpPacketQueue::enqueuePacket
  //
  // -----------------------------------------------------------------------------
  //
  void
    RtpPacketQueue::enqueuePacket(const unsigned char *data, int length, uint16_t nseq)
    {
      unsigned char *buf = new unsigned char[length];
      memcpy(buf, data, length);
      queue.insert(PACKETQUEUE::value_type(nseq, buf));
      lqueue.insert(LENGTHQ::value_type(nseq, length));
    }

  // -----------------------------------------------------------------------------
  // RtpPacketQueue::checkQueue
  //
  // -----------------------------------------------------------------------------
  //
  void
    RtpPacketQueue::checkQueue(void)
    {
      // Max size reached, send first
      if (queue.size() >= MAX_SIZE)
      {
        sendFirst();
      }
      // recorrer la cola para ver si hay paquetes que pueden ser enviados
      while (queue.size() > 0)
      {
        if (queue.begin()->first == lastNseq + 1)
        {
          sendFirst();
        }
        else
        {
          break;
        }
      }
    }

  // -----------------------------------------------------------------------------
  // RtpPacketQueue::cleanQueue
  //
  // -----------------------------------------------------------------------------
  //
  void
    RtpPacketQueue::cleanQueue(void)
    {
      // vaciar el mapa
      while (queue.size() > 0)
      {
        unsigned char *data = queue.begin()->second;
        queue.erase(queue.begin());
        delete[] data;
      }
      lqueue.clear();
    }

  // -----------------------------------------------------------------------------
  // RtpPacketQueue::sendFirst
  //
  // -----------------------------------------------------------------------------
  //
  void
    RtpPacketQueue::sendFirst(void)
    {
      unsigned char *data = queue.begin()->second;
      int length = lqueue.begin()->second;

      const RTPHeader *header = reinterpret_cast<const RTPHeader*>(data);
      lastNseq = queue.begin()->first;
      lastTs = header->getTimestamp();

      //channel->packetReceived2(data, length);
      queue.erase(queue.begin());
      lqueue.erase(lqueue.begin());

      delete []data;
    }

} /* namespace erizo */
