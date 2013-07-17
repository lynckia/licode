/*
 * rtputils.h
 */

#ifndef RTPUTILS_H_
#define RTPUTILS_H_

#include <netinet/in.h>

//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                           timestamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |           synchronization source (SSRC) identifier            |
//   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//   |            contributing source (CSRC) identifiers             |
//   |                             ....                              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

//   The first twelve octets are present in every RTP packet, while the
//   list of CSRC identifiers is present only when inserted by a mixer.
//   The fields have the following meaning:

//   version (V): 2 bits
//        This field identifies the version of RTP. The version defined by
//        this specification is two (2). (The value 1 is used by the first
//        draft version of RTP and the value 0 is used by the protocol
//        initially implemented in the "vat" audio tool.)

//   padding (P): 1 bit
//        If the padding bit is set, the packet contains one or more
//        additional padding octets at the end which are not part of the
//      payload. The last octet of the padding contains a count of how
//      many padding octets should be ignored. Padding may be needed by
//      some encryption algorithms with fixed block sizes or for
//      carrying several RTP packets in a lower-layer protocol data
//      unit.

// extension (X): 1 bit
//      If the extension bit is set, the fixed header is followed by
//      exactly one header extension, with a format defined in Section
//      5.3.1.

// CSRC count (CC): 4 bits
//      The CSRC count contains the number of CSRC identifiers that
//      follow the fixed header.

// marker (M): 1 bit
//      The interpretation of the marker is defined by a profile. It is
//      intended to allow significant events such as frame boundaries to
//      be marked in the packet stream. A profile may define additional
//      marker bits or specify that there is no marker bit by changing
//      the number of bits in the payload type field (see Section 5.3).

// payload type (PT): 7 bits
//      This field identifies the format of the RTP payload and
//      determines its interpretation by the application. A profile
//      specifies a default static mapping of payload type codes to
//      payload formats. Additional payload type codes may be defined
//      dynamically through non-RTP means (see Section 3). An initial
//      set of default mappings for audio and video is specified in the
//      companion profile Internet-Draft draft-ietf-avt-profile, and
//      may be extended in future editions of the Assigned Numbers RFC
//      [6].  An RTP sender emits a single RTP payload type at any given
//      time; this field is not intended for multiplexing separate media
//      streams (see Section 5.2).

// sequence number: 16 bits
//      The sequence number increments by one for each RTP data packet
//      sent, and may be used by the receiver to detect packet loss and
//      to restore packet sequence. The initial value of the sequence
//      number is random (unpredictable) to make known-plaintext attacks
//      on encryption more difficult, even if the source itself does not
//      encrypt, because the packets may flow through a translator that
//      does. Techniques for choosing unpredictable numbers are
//      discussed in [7].

// timestamp: 32 bits
//      The timestamp reflects the sampling instant of the first octet
//      in the RTP data packet. The sampling instant must be derived
//      from a clock that increments monotonically and linearly in time
//      to allow synchronization and jitter calculations (see Section
//      6.3.1).  The resolution of the clock must be sufficient for the
//      desired synchronization accuracy and for measuring packet
//      arrival jitter (one tick per video frame is typically not
//      sufficient).  The clock frequency is dependent on the format of
//      data carried as payload and is specified statically in the
//      profile or payload format specification that defines the format,
//      or may be specified dynamically for payload formats defined
//      through non-RTP means. If RTP packets are generated
//      periodically, the nominal sampling instant as determined from
//      the sampling clock is to be used, not a reading of the system
//      clock. As an example, for fixed-rate audio the timestamp clock
//      would likely increment by one for each sampling period.  If an
//      audio application reads blocks covering 160 sampling periods
//      from the input device, the timestamp would be increased by 160
//      for each such block, regardless of whether the block is
//      transmitted in a packet or dropped as silent.

// The initial value of the timestamp is random, as for the sequence
// number. Several consecutive RTP packets may have equal timestamps if
// they are (logically) generated at once, e.g., belong to the same
// video frame. Consecutive RTP packets may contain timestamps that are
// not monotonic if the data is not transmitted in the order it was
// sampled, as in the case of MPEG interpolated video frames. (The
// sequence numbers of the packets as transmitted will still be
// monotonic.)

// SSRC: 32 bits
//      The SSRC field identifies the synchronization source. This
//      identifier is chosen randomly, with the intent that no two
//      synchronization sources within the same RTP session will have
//      the same SSRC identifier. An example algorithm for generating a
//      random identifier is presented in Appendix A.6. Although the
//      probability of multiple sources choosing the same identifier is
//      low, all RTP implementations must be prepared to detect and
//      resolve collisions.  Section 8 describes the probability of
//      collision along with a mechanism for resolving collisions and
//      detecting RTP-level forwarding loops based on the uniqueness of
//      the SSRC identifier. If a source changes its source transport
//      address, it must also choose a new SSRC identifier to avoid
//      being interpreted as a looped source.

typedef struct {
    uint32_t cc :4;
    uint32_t extension :1;
    uint32_t padding :1;
    uint32_t version :2;
    uint32_t payloadtype :7;
    uint32_t marker :1;
    uint32_t seqnum :16;
    uint32_t timestamp;
    uint32_t ssrc;
} rtpheader;

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|    RC   |   PT=RR=201   |             length            | header
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     SSRC of packet sender                     |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |                 SSRC_1 (SSRC of first source)                 | report
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
// | fraction lost |       cumulative number of packets lost       |   1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           extended highest sequence number received           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                      interarrival jitter                      |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         last SR (LSR)                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                   delay since last SR (DLSR)                  |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |                 SSRC_2 (SSRC of second source)                | report
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
// :                               ...                             :   2
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |                  profile-specific extensions                  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

typedef struct {
    uint32_t blockcount :5;
    uint32_t padding :1;
    uint32_t version :2;
    uint32_t packettype :8;
    uint32_t length :16;
    uint32_t ssrc;
    uint32_t ssrcsource;
    uint32_t fractionLost:8;
} rtcpheader;

//     0                   1                    2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |F|   block PT  |  timestamp offset         |   block length    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//
// RFC 2198          RTP Payload for Redundant Audio Data    September 1997
//
//    The bits in the header are specified as follows:
//
//    F: 1 bit First bit in header indicates whether another header block
//        follows.  If 1 further header blocks follow, if 0 this is the
//        last header block.
//        If 0 there is only 1 byte RED header
//
//    block PT: 7 bits RTP payload type for this block.
//
//    timestamp offset:  14 bits Unsigned offset of timestamp of this block
//        relative to timestamp given in RTP header.  The use of an unsigned
//        offset implies that redundant data must be sent after the primary
//        data, and is hence a time to be subtracted from the current
//        timestamp to determine the timestamp of the data for which this
//        block is the redundancy.
//
//    block length:  10 bits Length in bytes of the corresponding data
//        block excluding header.
typedef struct {
    uint32_t payloadtype :7;
    uint32_t follow :1;
    uint32_t ts :14;
    uint32_t length :10;
} redheader;

// Payload types
#define RTCP_Sender_PT      200 // RTCP Sender Report
#define RTCP_Receiver_PT    201 // RTCP Sender Report
#define RTCP_Feedback_PT    206 // RTCP Feddback Packet

#define VP8_90000_PT        100 // VP8 Video Codec
#define RED_90000_PT        116 // REDundancy (RFC 2198)
#define ULP_90000_PT        117 // ULP/FEC
#define ISAC_16000_PT       103 // ISAC Audio Codec
#define ISAC_32000_PT       104 // ISAC Audio Codec
#define PCMU_8000_PT        0   // PCMU Audio Codec
#define OPUS_48000_PT       111 // Opus Audio Codec
#define PCMA_8000_PT        8   // PCMA Audio Codec
#define CN_8000_PT          13  // CN Audio Codec
#define CN_16000_PT         105 // CN Audio Codec
#define CN_32000_PT         106 // CN Audio Codec
#define CN_48000_PT         107 // CN Audio Codec
#define TEL_8000_PT         126 // Tel Audio Events

#endif /* RTPUTILS_H */