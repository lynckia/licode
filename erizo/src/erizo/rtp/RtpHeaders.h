/*
 * RtpHeaders.h
 */

#ifndef RTPHEADERS_H_
#define RTPHEADERS_H_

#include <netinet/in.h>

namespace erizo{
  // Payload types
#define RTCP_Sender_PT       200 // RTCP Sender Report
#define RTCP_Receiver_PT     201 // RTCP Receiver Report
#define RTCP_SDES_PT         202
#define RTCP_BYE             203
#define RTCP_APP             204 
#define RTCP_RTP_Feedback_PT 205 // RTCP Transport Layer Feedback Packet
#define RTCP_PS_Feedback_PT  206 // RTCP Payload Specific Feedback Packet

#define RTCP_PLI_FMT           1
#define RTCP_SLI_FMT           2
#define RTCP_FIR_FMT           4
#define RTCP_AFB              15

#define VP8_90000_PT        100 // VP8 Video Codec
#define RED_90000_PT        116 // REDundancy (RFC 2198)
#define RTX_90000_PT        96  // RTX packet
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
      uint32_t hasextension :1;
      uint32_t padding :1;
      uint32_t version :2;
      uint32_t payloadtype :7;
      uint32_t marker :1;
      uint32_t seqnum :16;
      uint32_t timestamp;
      uint32_t ssrc;
      uint32_t extensionpayload:16;
      uint32_t extensionlength:16;
/*    RFC 5285
        0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |       0xBE    |    0xDE       |           length=3            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  ID   | L=0   |     data      |  ID   |  L=1  |   data...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            ...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          data                                 |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/
      uint32_t extensions;

      inline RtpHeader() :
        cc(0), hasextension(0), padding(0), version(2), payloadtype(0), marker(
            0), seqnum(0), timestamp(0), ssrc(0), extensionpayload(0), extensionlength(0) {
          // No implementation required
      }

      inline uint8_t hasPadding() const {
        return padding;
      }

      inline uint8_t getVersion() const {
        return version;
      }
      inline void setVersion(uint8_t aVersion) {
        version = aVersion;
      }
      inline uint8_t getMarker() const {
        return marker;
      }
      inline void setMarker(uint8_t aMarker) {
        marker = aMarker;
      }
      inline uint8_t getExtension() const {
        return hasextension;
      }
      inline void setExtension(uint8_t ext) {
        hasextension = ext;
      }
      inline uint8_t getCc() const {
        return cc;
      }
      inline void setCc (uint8_t theCc){
        cc = theCc;
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
        return MIN_SIZE + cc * 4 + hasextension * (4 + ntohs(extensionlength) * 4);
      }
  };

  class AbsSendTimeExtension {
    public:
      uint32_t ext_info:8;
      uint32_t abs_data:24;
      inline uint8_t getId(){
        return ext_info >> 4;
      }
      inline uint8_t getLength(){
        return (ext_info & 0x0F);
      }
      inline uint32_t getAbsSendTime() {
        return ntohl(abs_data)>>8;
      }
      inline void setAbsSendTime(uint32_t aTime) {
        abs_data = htonl(aTime)>>8;
      }
  };

  class RtpRtxHeader {
    public:

      RtpHeader rtpHeader;
      uint16_t osn;

      inline uint16_t getOsn(){
        return ntohs (osn);
      }
      inline void setOs (uint16_t theOsn){
        osn = htons (theOsn);
      }
  };

  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |V=2|P|    RC   |   PT=RR=201   |             length            | header
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                     SSRC of packet sender                     |
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  //
  // RECEIVER REPORT
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
  //
  // SENDER REPORT // PT = 200
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |            NTP timestamp, most significant word NTS           |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |             NTP timestamp, least significant word             |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                       RTP timestamp RTS                       |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                   sender's packet count SPC                   |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                    sender's octet count SOC                   |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // 0                   1                   2                   3
  // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |V=2|P|    SC   |  PT=SDES=202  |             length            | header
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  // |                          SSRC/CSRC_1                          | chunk 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                           SDES items                          |
  // |                              ...                              |
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  // |                          SSRC/CSRC_2                          | chunk 2
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                           SDES items                          |
  // |                              ...                              |
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  //
  //
  //
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |V=2|P| FMT=15  |   PT=206      |             length            |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                  SSRC of packet sender                        |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                  SSRC of media source                         |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |  Unique identifier 'R' 'E' 'M' 'B'                            |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |  Num SSRC     | BR Exp    |  BR Mantissa                      | max= mantissa*2^exp
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |   SSRC feedback                                               |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |  ...                                                          |

  class RtcpHeader {
    public:
      uint32_t blockcount :5;
      uint32_t padding :1;
      uint32_t version :2;
      uint32_t packettype :8;
      uint32_t length :16;
      uint32_t ssrc;
      union report_t {
        struct receiverReport_t {
          uint32_t ssrcsource;
          /* RECEIVER REPORT DATA*/
          uint32_t fractionlost:8;
          int32_t lost:24;
          uint32_t seqnumcycles:16;
          uint32_t highestseqnum:16;
          uint32_t jitter;
          uint32_t lastsr;
          uint32_t delaysincelast;
        } receiverReport;

        struct senderReport_t {
          uint64_t ntptimestamp;
          uint32_t rtprts;
          uint32_t packetsent;
          uint32_t octetssent;
          struct receiverReport_t rrlist[1];
        } senderReport;
// Generic NACK RTCP_RTP_FB + (FMT 1)rfc4585
//      0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            PID                |             BLP               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        struct genericNack_t{
          uint32_t ssrcsource;
          uint32_t pid:16;
          uint32_t blp:16;
        } nackPacket;
        
        struct remb_t{
          uint32_t ssrcsource;
          uint32_t uniqueid;
          uint32_t numssrc:8;
          uint32_t brLength :24;
          uint32_t ssrcfeedb;

        } rembPacket;
        
        struct pli_t{
          uint32_t ssrcsource;
          uint32_t fci;
        } pli;

      } report;
      inline RtcpHeader(): blockcount(0), padding(0), version(2), packettype (0), length(0),
     ssrc(0){
      };
      inline bool isFeedback(void) {
        return (packettype==RTCP_Receiver_PT || 
            packettype==RTCP_PS_Feedback_PT ||
            packettype == RTCP_RTP_Feedback_PT);
      }
      inline bool isRtcp(void) {        
        return (packettype == RTCP_Sender_PT ||
            packettype == RTCP_APP ||
            isFeedback()            
            );
      }
      inline uint8_t getPacketType(){
        return packettype;
      }
      inline void setPacketType(uint8_t pt){
        packettype = pt;
      }
      inline uint8_t getBlockCount(){
        return (uint8_t)blockcount;
      }
      inline void setBlockCount(uint8_t count){
        blockcount = count;
      }
      inline uint16_t getLength() {
        return ntohs(length);
      }
      inline void setLength(uint16_t theLength) {
        length = htons(theLength);
      }
      inline uint32_t getSSRC(){
        return ntohl(ssrc);
      }
      inline void setSSRC(uint32_t aSsrc){
        ssrc = htonl(aSsrc);
      }
      inline uint32_t getSourceSSRC(){
        return ntohl(report.receiverReport.ssrcsource);
      }
      inline void setSourceSSRC(uint32_t sourceSsrc){
        report.receiverReport.ssrcsource = htonl(sourceSsrc);
      }
      inline uint8_t getFractionLost() {
        return (uint8_t)report.receiverReport.fractionlost;
      }
      inline void setFractionLost(uint8_t fractionLost){
        report.receiverReport.fractionlost = fractionLost;
      }
      inline uint32_t getLostPackets() {
        return ntohl(report.receiverReport.lost)>>8;
      }
      inline void setLostPackets(uint32_t lost) {
        report.receiverReport.lost = htonl(lost)>>8;
      }
      inline uint16_t getSeqnumCycles() {
        return ntohs(report.receiverReport.seqnumcycles);
      }
      inline void setSeqnumCycles(uint16_t seqnumcycles) {
        report.receiverReport.seqnumcycles = htons(seqnumcycles);
      }
      inline uint16_t getHighestSeqnum() {
        return ntohs(report.receiverReport.highestseqnum);
      }
      inline void setHighestSeqnum(uint16_t highest) {
        report.receiverReport.highestseqnum = htons(highest);
      }
      inline uint32_t getJitter() {
        return ntohl(report.receiverReport.jitter);
      }
      inline void setJitter(uint32_t jitter) {
        report.receiverReport.jitter = htonl(jitter);
      }
      inline uint32_t getLastSr(){
        return ntohl(report.receiverReport.lastsr);
      }
      inline void setLastSr(uint32_t lastsr) {
        report.receiverReport.lastsr = htonl(lastsr);
      }
      inline uint32_t getDelaySinceLastSr(){
        return ntohl (report.receiverReport.delaysincelast);
      }
      inline void setDelaySinceLastSr(uint32_t delaylastsr) {
        report.receiverReport.delaysincelast = htonl(delaylastsr);
      }

      inline uint32_t getPacketsSent(){
        return ntohl(report.senderReport.packetsent);
      }
      inline void setPacketsSent(uint32_t packetssent){
        report.senderReport.packetsent = htonl(packetssent);
      }
      inline uint32_t getOctetsSent(){
        return ntohl(report.senderReport.octetssent);
      }      
      inline uint64_t getNtpTimestamp(){
       return (((uint64_t)htonl(report.senderReport.ntptimestamp)) << 32) + htonl(report.senderReport.ntptimestamp >> 32);
      }
      inline uint16_t getNackPid(){
        return ntohs(report.nackPacket.pid);
      }
      inline void setNackPid(uint16_t pid){
        report.nackPacket.pid = htons(pid);
      }
      inline uint16_t getNackBlp(){
        return ntohs(report.nackPacket.blp);
      }
      inline void setNackBlp(uint16_t blp){
        report.nackPacket.blp = htons(blp);
      }
      inline void setREMBBitRate(uint64_t bitRate){
        uint64_t max = 0x3FFFF; // 18 bits
        uint16_t exp = 0;
        while ( bitRate >= max && exp < 64){
          exp+=1;
          max = max << 1;
        }
        uint64_t mantissa = bitRate >> exp;
        exp = exp&0x3F;
        mantissa = mantissa&0x3FFFF;
        uint32_t line = mantissa + (exp << 18);
        report.rembPacket.brLength = htonl(line)>>8;

      }
      inline uint32_t getBrExp(){ 
        //remove the 0s added by nothl (8) + the 18 bits of Mantissa 
        return (ntohl(report.rembPacket.brLength)>>26);
      }
      inline uint32_t getBrMantis(){        
        return (ntohl(report.rembPacket.brLength)>>8 & 0x3ffff);
      }
      inline uint8_t getREMBNumSSRC(){
        return report.rembPacket.numssrc;
      }
      inline void setREMBNumSSRC(uint8_t num){
        report.rembPacket.numssrc = num;
      }
      inline uint32_t getREMBFeedSSRC(){
        return ntohl(report.rembPacket.ssrcfeedb);
      }
      inline void setREMBFeedSSRC(uint32_t ssrc){
         report.rembPacket.ssrcfeedb = htonl(ssrc);
      }
      inline uint32_t getFCI(){
        return ntohl(report.pli.fci);
      }
      inline void setFCI(uint32_t fci){
        report.pli.fci = htonl(fci);
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
        // remove the 8 bits added by nothl + the 10 from length 
        return (ntohl(tsLength) & 0xfffc0000) >> 18;
      }
      uint32_t getLength() {
        return (ntohl(tsLength) & 0x3ff00);
      }
  };
} /*namespace erizo*/

#endif /* RTPHEADERS_H_ */
