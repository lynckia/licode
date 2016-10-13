/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ERIZO_SRC_ERIZO_RTP_WEBRTC_FEC_RECEIVER_IMPL_H_
#define ERIZO_SRC_ERIZO_RTP_WEBRTC_FEC_RECEIVER_IMPL_H_

// This header is included to get the nested declaration of Packet structure.

// #include "webrtc/modules/rtp_rtcp/interface/fec_receiver.h"
// #include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "rtp/webrtc/forward_error_correction.h"
#include "rtp/webrtc/rtp_utility.h"
#include "boost/thread.hpp"
// #include "webrtc/typedefs.h"

namespace webrtc {

class RtpData {
 public:
    virtual ~RtpData() {}

    virtual int32_t OnReceivedPayloadData(
        const uint8_t* payloadData,
        const uint16_t payloadSize,
        const WebRtcRTPHeader* rtpHeader) = 0;

    virtual bool OnRecoveredPacket(const uint8_t* packet,
                                   int packet_length) = 0;
};

class CriticalSectionWrapper;

class FecReceiverImpl {
 public:
  explicit FecReceiverImpl(RtpData* callback);
  virtual ~FecReceiverImpl();

  int32_t AddReceivedRedPacket(const RTPHeader& rtp_header,
                                       const uint8_t* incoming_rtp_packet,
                                       int packet_length,
                                       uint8_t ulpfec_payload_type);

  int32_t ProcessReceivedFec();

 private:
  boost::mutex crit_sect_;
  RtpData* recovered_packet_callback_;
  ForwardErrorCorrection* fec_;
  // TODO(holmer): In the current version received_packet_list_ is never more
  // than one packet, since we process FEC every time a new packet
  // arrives. We should remove the list.
  ForwardErrorCorrection::ReceivedPacketList received_packet_list_;
  ForwardErrorCorrection::RecoveredPacketList recovered_packet_list_;
};
}  // namespace webrtc

#endif  // ERIZO_SRC_ERIZO_RTP_WEBRTC_FEC_RECEIVER_IMPL_H_
