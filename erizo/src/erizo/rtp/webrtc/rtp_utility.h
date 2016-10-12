/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ERIZO_SRC_ERIZO_RTP_WEBRTC_RTP_UTILITY_H_
#define ERIZO_SRC_ERIZO_RTP_WEBRTC_RTP_UTILITY_H_

#include <stddef.h>  // size_t, ptrdiff_t
#include <string.h>
#include <stdint.h>

// #include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
// #include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
// #include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
// #include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
// #include "webrtc/typedefs.h"

namespace webrtc {

const uint8_t kRtpMarkerBitMask = 0x80;


// RTP
enum {kRtpCsrcSize = 15};  // RFC 3550 page 13

struct RTPHeaderExtension {
  RTPHeaderExtension()
      : hasTransmissionTimeOffset(false),
        transmissionTimeOffset(0),
        hasAbsoluteSendTime(false),
        absoluteSendTime(0),
        hasAudioLevel(false),
        audioLevel(0) {}

  bool hasTransmissionTimeOffset;
  int32_t transmissionTimeOffset;
  bool hasAbsoluteSendTime;
  uint32_t absoluteSendTime;

  // Audio Level includes both level in dBov and voiced/unvoiced bit. See:
  // https:// datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/
  bool hasAudioLevel;
  uint8_t audioLevel;
};

struct RTPHeader {
  RTPHeader()
      : markerBit(false),
        payloadType(0),
        sequenceNumber(0),
        timestamp(0),
        ssrc(0),
        numCSRCs(0),
        paddingLength(0),
        headerLength(0),
        payload_type_frequency(0),
        extension() {
    memset(&arrOfCSRCs, 0, sizeof(arrOfCSRCs));
  }

  bool markerBit;
  uint8_t payloadType;
  uint16_t sequenceNumber;
  uint32_t timestamp;
  uint32_t ssrc;
  uint8_t numCSRCs;
  uint32_t arrOfCSRCs[kRtpCsrcSize];
  uint8_t paddingLength;
  uint16_t headerLength;
  int payload_type_frequency;
  RTPHeaderExtension extension;
};

enum FrameType {
    kFrameEmpty            = 0,
    kAudioFrameSpeech      = 1,
    kAudioFrameCN          = 2,
    kVideoFrameKey         = 3,    // independent frame
    kVideoFrameDelta       = 4     // depends on the previus frame
};

struct RTPAudioHeader {
  uint8_t numEnergy;                  // number of valid entries in arrOfEnergy
  uint8_t arrOfEnergy[kRtpCsrcSize];  // one energy byte (0-9) per channel
  bool isCNG;                         // is this CNG
  uint8_t channel;                    // number of channels 2 = stereo
};

const int16_t kNoPictureId = -1;
const int16_t kNoTl0PicIdx = -1;
const uint8_t kNoTemporalIdx = 0xFF;
const int kNoKeyIdx = -1;

struct RTPVideoHeaderVP8 {
  void InitRTPVideoHeaderVP8() {
    nonReference = false;
    pictureId = kNoPictureId;
    tl0PicIdx = kNoTl0PicIdx;
    temporalIdx = kNoTemporalIdx;
    layerSync = false;
    keyIdx = kNoKeyIdx;
    partitionId = 0;
    beginningOfPartition = false;
  }

  bool nonReference;          // Frame is discardable.
  int16_t pictureId;          // Picture ID index, 15 bits;
                              // kNoPictureId if PictureID does not exist.
  int16_t tl0PicIdx;          // TL0PIC_IDX, 8 bits;
                              // kNoTl0PicIdx means no value provided.
  uint8_t temporalIdx;        // Temporal layer index, or kNoTemporalIdx.
  bool layerSync;             // This frame is a layer sync frame.
                              // Disabled if temporalIdx == kNoTemporalIdx.
  int keyIdx;                 // 5 bits; kNoKeyIdx means not used.
  int partitionId;            // VP8 partition ID
  bool beginningOfPartition;  // True if this packet is the first
                              // in a VP8 partition. Otherwise false
};

struct RTPVideoHeaderH264 {
  bool stap_a;
  bool single_nalu;
};

union RTPVideoTypeHeader {
  RTPVideoHeaderVP8 VP8;
  RTPVideoHeaderH264 H264;
};

enum RtpVideoCodecTypes {
  kRtpVideoNone,
  kRtpVideoGeneric,
  kRtpVideoVp8,
  kRtpVideoH264
};
struct RTPVideoHeader {
  uint16_t width;  // size
  uint16_t height;

  bool isFirstPacket;    // first packet in frame
  uint8_t simulcastIdx;  // Index if the simulcast encoder creating
                         // this frame, 0 if not using simulcast.
  RtpVideoCodecTypes codec;
  RTPVideoTypeHeader codecHeader;
};

union RTPTypeHeader {
  RTPAudioHeader Audio;
  RTPVideoHeader Video;
};

struct WebRtcRTPHeader {
  RTPHeader header;
  FrameType frameType;
  RTPTypeHeader type;
  // NTP time of the capture time in local timebase in milliseconds.
  int64_t ntp_time_ms;
};

// RtpData* NullObjectRtpData();
// RtpFeedback* NullObjectRtpFeedback();
// RtpAudioFeedback* NullObjectRtpAudioFeedback();
// ReceiveStatistics* NullObjectReceiveStatistics();

namespace RtpUtility {
//    // January 1970, in NTP seconds.
//    const uint32_t NTP_JAN_1970 = 2208988800UL;

//    // Magic NTP fractional unit.
//    const double NTP_FRAC = 4.294967296E+9;

//    struct Payload
//    {
//        char name[RTP_PAYLOAD_NAME_SIZE];
//        bool audio;
//        PayloadUnion typeSpecific;
//    };

//    typedef std::map<int8_t, Payload*> PayloadTypeMap;

//    // Return the current RTP timestamp from the NTP timestamp
//    // returned by the specified clock.
//    uint32_t GetCurrentRTP(Clock* clock, uint32_t freq);

//    // Return the current RTP absolute timestamp.
//    uint32_t ConvertNTPTimeToRTP(uint32_t NTPsec,
//                                 uint32_t NTPfrac,
//                                 uint32_t freq);

//    uint32_t pow2(uint8_t exp);

//    // Returns true if |newTimestamp| is older than |existingTimestamp|.
//    // |wrapped| will be set to true if there has been a wraparound between the
//    // two timestamps.
//    bool OldTimestamp(uint32_t newTimestamp,
//                      uint32_t existingTimestamp,
//                      bool* wrapped);

//    bool StringCompare(const char* str1,
//                       const char* str2,
//                       const uint32_t length);

    void AssignUWord32ToBuffer(uint8_t* dataBuffer, uint32_t value);
    void AssignUWord24ToBuffer(uint8_t* dataBuffer, uint32_t value);
    void AssignUWord16ToBuffer(uint8_t* dataBuffer, uint16_t value);

    /**
     * Converts a network-ordered two-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered two-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint16_t BufferToUWord16(const uint8_t* dataBuffer);

    /**
     * Converts a network-ordered three-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered three-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint32_t BufferToUWord24(const uint8_t* dataBuffer);

    /**
     * Converts a network-ordered four-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered four-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint32_t BufferToUWord32(const uint8_t* dataBuffer);

//    class RtpHeaderParser {
//    public:
//     RtpHeaderParser(const uint8_t* rtpData, size_t rtpDataLength);
//     ~RtpHeaderParser();

//        bool RTCP() const;
//        bool ParseRtcp(RTPHeader* header) const;
//        bool Parse(RTPHeader& parsedPacket,
//                   RtpHeaderExtensionMap* ptrExtensionMap = NULL) const;

//    private:
//        void ParseOneByteExtensionHeader(
//            RTPHeader& parsedPacket,
//            const RtpHeaderExtensionMap* ptrExtensionMap,
//            const uint8_t* ptrRTPDataExtensionEnd,
//            const uint8_t* ptr) const;

//        uint8_t ParsePaddingBytes(
//            const uint8_t* ptrRTPDataExtensionEnd,
//            const uint8_t* ptr) const;

//        const uint8_t* const _ptrRTPDataBegin;
//        const uint8_t* const _ptrRTPDataEnd;
//    };

//    enum FrameTypes
//    {
//        kIFrame,    // key frame
//        kPFrame         // Delta frame
//    };

//    struct RTPPayloadVP8
//    {
//        bool                 nonReferenceFrame;
//        bool                 beginningOfPartition;
//        int                  partitionID;
//        bool                 hasPictureID;
//        bool                 hasTl0PicIdx;
//        bool                 hasTID;
//        bool                 hasKeyIdx;
//        int                  pictureID;
//        int                  tl0PicIdx;
//        int                  tID;
//        bool                 layerSync;
//        int                  keyIdx;
//        int                  frameWidth;
//        int                  frameHeight;

//        const uint8_t*   data;
//        uint16_t         dataLength;
//    };

//    union RTPPayloadUnion
//    {
//        RTPPayloadVP8   VP8;
//    };

//    struct RTPPayload
//    {
//        void SetType(RtpVideoCodecTypes videoType);

//        RtpVideoCodecTypes  type;
//        FrameTypes          frameType;
//        RTPPayloadUnion     info;
//    };

//    // RTP payload parser
//    class RTPPayloadParser
//    {
//    public:
//        RTPPayloadParser(const RtpVideoCodecTypes payloadType,
//                         const uint8_t* payloadData,
//                         // Length w/o padding.
//                         const uint16_t payloadDataLength);

//        ~RTPPayloadParser();

//        bool Parse(RTPPayload& parsedPacket) const;

//    private:
//        bool ParseGeneric(RTPPayload& parsedPacket) const;

//        bool ParseVP8(RTPPayload& parsedPacket) const;

//        int ParseVP8Extension(RTPPayloadVP8 *vp8,
//                              const uint8_t *dataPtr,
//                              int dataLength) const;

//        int ParseVP8PictureID(RTPPayloadVP8 *vp8,
//                              const uint8_t **dataPtr,
//                              int *dataLength,
//                              int *parsedBytes) const;

//        int ParseVP8Tl0PicIdx(RTPPayloadVP8 *vp8,
//                              const uint8_t **dataPtr,
//                              int *dataLength,
//                              int *parsedBytes) const;

//        int ParseVP8TIDAndKeyIdx(RTPPayloadVP8 *vp8,
//                                 const uint8_t **dataPtr,
//                                 int *dataLength,
//                                 int *parsedBytes) const;

//        int ParseVP8FrameSize(RTPPayload& parsedPacket,
//                              const uint8_t *dataPtr,
//                              int dataLength) const;

//    private:
//        const uint8_t*        _dataPtr;
//        const uint16_t        _dataLength;
//        const RtpVideoCodecTypes    _videoType;
//    };

    }  // namespace RtpUtility

}  // namespace webrtc

#endif  // ERIZO_SRC_ERIZO_RTP_WEBRTC_RTP_UTILITY_H_
