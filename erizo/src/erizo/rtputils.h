/*
 * rtputils.h
 */

#ifndef RTPUTILS_H_
#define RTPUTILS_H_

#include <netinet/in.h>

namespace erizo{
  // Payload types
#define RTCP_Sender_PT      200 // RTCP Sender Report
#define RTCP_Receiver_PT    201 // RTCP Receiver Report
#define RTCP_RTP_Feedback_PT 205 // RTCP Transport Layer Feedback Packet
#define RTCP_PS_Feedback_PT    206 // RTCP Payload Specific Feedback Packet

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


  // 0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |      defined by profile       |           length              |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                        header extension                       |
  // |                             ....                              |

  class RtpHeader {
    public:
      static const int MIN_SIZE = 12;
      uint32_t cc :4;
      uint32_t extension :1;
      uint32_t padding :1;
      uint32_t version :2;
      uint32_t payloadtype :7;
      uint32_t marker :1;
      uint32_t seqnum :16;
      uint32_t timestamp;
      uint32_t ssrc;
      uint32_t extensionpayload:16;
      uint32_t extensionlength:16;

      inline uint8_t getMarker() const {
        return marker;
      }
      inline void setMarker(uint8_t aMarker) {
        marker = aMarker;
      }
      inline uint8_t getExtension() const {
        return extension;
      }
      inline void setExtension(uint8_t ext) {
        extension = ext;
      }
      inline uint8_t getPayloadType() const {
        return payloadtype;
      }
      inline void setPayloadType(uint8_t aType) {
        payloadtype = aType;
      }
      inline uint16_t getSeqNumber() const {
        return ntohs(seqnum);
      }
      inline void setSeqNumber(uint16_t aSeqNumber) {
        seqnum = htons(aSeqNumber);
      }
      inline uint32_t getTimestamp() const {
        return ntohl(timestamp);
      }
      inline void setTimestamp(uint32_t aTimestamp) {
        timestamp = htonl(aTimestamp);
      }
      inline uint32_t getSSRC() const {
        return ntohl(ssrc);
      }
      inline void setSSRC(uint32_t aSSRC) {
        ssrc = htonl(aSSRC);
      }
      inline uint16_t getExtId() const {
        return ntohs(extensionpayload);
      }
      inline void setExtId(uint16_t extensionId) {
        extensionpayload = htons(extensionId);
      }
      inline uint16_t getExtLength() const {
        return ntohs(extensionlength);
      }
      inline void setExtLength(uint16_t extensionLength) {
        extensionlength = htons(extensionLength);
      }
      inline int getHeaderLength() {
        return MIN_SIZE + cc * 4 + extension * (4 + ntohs(extensionlength) * 4);
      }
  };

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

  class RtcpHeader {
    public:
      uint32_t blockcount :5;
      uint32_t padding :1;
      uint32_t version :2;
      uint32_t packettype :8;
      uint32_t length :16;
      uint32_t ssrc;
      uint32_t ssrcsource;
      /* RECEIVER REPORT DATA*/
      uint32_t fractionlost:8;
      int32_t lost:24;
      uint32_t highestseqnum;
      uint32_t jitter;
      uint32_t lastSR;
      
      inline bool isFeedback(void){
        return (packettype==RTCP_Receiver_PT || 
            packettype==RTCP_PS_Feedback_PT ||
            packettype == RTCP_RTP_Feedback_PT);
      }
      inline bool isRtcp(void){        
        return (packettype == RTCP_Sender_PT || 
            packettype == RTCP_Receiver_PT || 
            packettype == RTCP_PS_Feedback_PT||
            packettype == RTCP_RTP_Feedback_PT);
      }
      inline int getLost(){
        return ntohl(lost);
      }
      inline uint32_t getHighestSeqnum(){
        return ntohl(highestseqnum);
      }
      inline uint32_t getJitter(){
        return ntohl(jitter);
      }
  };


  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |V=2|P|   FMT   |       PT      |          length               |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                  SSRC of packet sender                        |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                  SSRC of media source                         |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   :            Feedback Control Information (FCI)                 :
  //   :                                                               :

  //   The Feedback Control Information (FCI) for the Full Intra Request
  //   consists of one or more FCI entries, the content of which is depicted
  //   in Figure 4.  The length of the FIR feedback message MUST be set to
  //   2+2*N, where N is the number of FCI entries.
  //
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                              SSRC                             |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   | Seq nr.       |    Reserved                                   |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


  class FirHeader {
    public: 
      uint32_t fmt :5;
      uint32_t padding :1;
      uint32_t version :2;
      uint32_t packettype :8;
      uint32_t length :16;
      uint32_t ssrc;
      uint32_t ssrcofmediasource;
      uint32_t ssrc_fir;
  };


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
  class RedHeader {
    public:
      uint32_t payloadtype :7;
      uint32_t follow :1;
      uint32_t tsLength :24;
      uint32_t getTS() {
        return (ntohl(tsLength) & 0xfffc00) >> 10;
      }
      uint32_t getLength() {
        return (ntohl(tsLength) & 0x3ff);
      }
  };
} /*namespace erizo*/

#endif /* RTPUTILS_H */
