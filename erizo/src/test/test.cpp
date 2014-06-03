#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE erizo
#include <boost/test/unit_test.hpp>

// Headers for RtpPacketQueue.h tests
#include <media/rtp/RtpPacketQueue.h>
#include <media/rtp/RtpHeader.h>
#include <MediaDefinitions.h>

/*---------- RtpPacketQueue TESTS ----------*/
BOOST_AUTO_TEST_CASE(rtpPacketQueueDefaults)
{
    erizo::RtpPacketQueue queue;
    BOOST_CHECK(queue.getSize() == 0);
    BOOST_CHECK(queue.hasData() == false);
}

BOOST_AUTO_TEST_CASE(rtpPacketQueueBasicBehavior)
{
    erizo::RtpPacketQueue queue;

    RTPHeader header;
    header.setSeqNumber(12);
    queue.pushPacket((const char *)&header, sizeof(RTPHeader));

    // We should have one packet, and we're not ready to pop any data.
    BOOST_CHECK(queue.getSize() == 1);
    BOOST_CHECK(queue.hasData() == false);

    // If we try to pop this packet, we should get back a null shared_ptr object.
    boost::shared_ptr<erizo::dataPacket> packet = queue.popPacket();
    BOOST_CHECK(packet == 0);
    BOOST_CHECK(queue.getSize() == 1);
    BOOST_CHECK(queue.hasData() == false);

    // If we use the override in popPacket, we should get back header.
    packet = queue.popPacket(true);
    const RTPHeader *poppedHeader = reinterpret_cast<const RTPHeader*>(packet->data);
    BOOST_CHECK(poppedHeader->getSeqNumber() == 12);

    // Validate our size and that we have no data to offer.
    BOOST_CHECK(queue.getSize() == 0);
    BOOST_CHECK(queue.hasData() == false);
}

BOOST_AUTO_TEST_CASE(rtpPacketQueueCorrectlyReordersSamples)
{
    erizo::RtpPacketQueue queue;
    // Add sequence numbers 10 9 8 7 6 5 4 3 2 1.
    for(int x = 10; x >0; x--) {
        RTPHeader header;
        header.setSeqNumber(x);
        queue.pushPacket((const char *)&header, sizeof(RTPHeader));
    }

    for(int x = 1; x <=10; x++) {
        boost::shared_ptr<erizo::dataPacket> packet = queue.popPacket(true); // override our default pop behavior so we can validate these are ordered
        const RTPHeader *poppedHeader = reinterpret_cast<const RTPHeader*>(packet->data);
        BOOST_CHECK(poppedHeader->getSeqNumber() == x);
    }
}

BOOST_AUTO_TEST_CASE(rtpPacketQueueCorrectlyHandlesSequenceNumberRollover)
{
    erizo::RtpPacketQueue queue;
    // We'll start at 65530 and add one to an unsigned integer until we have overflow
    // samples.
    uint16_t x = 65530;
    while(x != 5) {
        RTPHeader header;
        header.setSeqNumber(x);
        x += 1;
        queue.pushPacket((const char *)&header, sizeof(RTPHeader));
    }

    x = 65530;
    while( x != 5) {
        boost::shared_ptr<erizo::dataPacket> packet = queue.popPacket(true); // override our default pop behavior so we can validate these are ordered
        const RTPHeader *poppedHeader = reinterpret_cast<const RTPHeader*>(packet->data);
        BOOST_CHECK(poppedHeader->getSeqNumber() == x);
        x += 1;
    }

    BOOST_CHECK(queue.getSize() == 0);
}

BOOST_AUTO_TEST_CASE(rtpPacketQueueCorrectlyHandlesSequenceNumberRolloverBackwards)
{
    erizo::RtpPacketQueue queue;
    // We'll start at 5 and subtract till we're at 65530.  Then make sure we get back what we put in.
    uint16_t x = 4;     // has to be 4, or we have an off-by-one error
    while(x != 65529) { // has to be 29, or have have an off-by-one error
        RTPHeader header;
        header.setSeqNumber(x);
        x -= 1;
        queue.pushPacket((const char *)&header, sizeof(RTPHeader));
    }

    x = 65530;
    while( x != 5) {
        boost::shared_ptr<erizo::dataPacket> packet = queue.popPacket(true); // override our default pop behavior so we can validate these are ordered
        const RTPHeader *poppedHeader = reinterpret_cast<const RTPHeader*>(packet->data);
        BOOST_CHECK(poppedHeader->getSeqNumber() == x);
        x += 1;
    }

    BOOST_CHECK(queue.getSize() == 0);
}

BOOST_AUTO_TEST_CASE(rtpPacketQueueDoesNotPushSampleLessThanWhatHasBeenPopped)
{
    erizo::RtpPacketQueue queue;

    RTPHeader header;
    header.setSeqNumber(12);
    queue.pushPacket((const char *)&header, sizeof(RTPHeader));

    // Pop this packet
    queue.popPacket(true);

    // Then try to add a packet with a lower sequence number.  This packet should not have
    // been added to the queue.
    header.setSeqNumber(11);
    queue.pushPacket((const char *)&header, sizeof(RTPHeader));
    BOOST_CHECK(queue.getSize() == 0);
    BOOST_CHECK(queue.hasData() == false);
}

BOOST_AUTO_TEST_CASE(rtpPacketQueueMakesDataAvailableOnceEnoughSamplesPushed)
{
    unsigned int max = 10, depth = 5;
    erizo::RtpPacketQueue queue(max, depth);  // max and depth.

    uint16_t x = 0;

    for(x = 0; x < (depth - 1); x++) {
        RTPHeader header;
        header.setSeqNumber(x);
        queue.pushPacket((const char *)&header, sizeof(RTPHeader));
    }

    // Should have (depth - 1) samples.  We should not be ready yet.
    BOOST_CHECK(queue.getSize() == (depth - 1));
    BOOST_CHECK(queue.hasData() == false);

    // Add one more sample, verify that we have 5, and hasData now returns true.
    RTPHeader header;
    header.setSeqNumber(++x);
    queue.pushPacket((const char *)&header, sizeof(RTPHeader));

    BOOST_CHECK(queue.getSize() == depth);
    BOOST_CHECK(queue.hasData() == true);
}

BOOST_AUTO_TEST_CASE(rtpPacketQueueRespectsMax)
{
    unsigned int max = 10, depth = 5;
    erizo::RtpPacketQueue queue(max, depth);  // max and depth.

    uint16_t x = 0;

    // let's push ten times the max.
    for(uint16_t x = 0; x < (max * 10); x++) {
        RTPHeader header;
        header.setSeqNumber(x);
        queue.pushPacket((const char *)&header, sizeof(RTPHeader));
    }

    BOOST_CHECK(queue.getSize() == max);
    BOOST_CHECK(queue.hasData() == true);
}
