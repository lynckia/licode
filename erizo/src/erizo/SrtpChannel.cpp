/*
 * Srtpchannel.cpp
 */

#include <srtp2/srtp.h>

#include <string>

#include "SrtpChannel.h"
#include "lib/Base64.h"

using erizo::Base64;

namespace erizo {
DEFINE_LOGGER(SrtpChannel, "SrtpChannel");
bool SrtpChannel::initialized = false;
boost::mutex SrtpChannel::sessionMutex_;

constexpr int kKeyStringLength = 32;

uint8_t nibble_to_hex_char(uint8_t nibble) {
  char buf[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                   '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

  return buf[nibble & 0xF];
}

std::string octet_string_hex_string(const void *s, int length) {
  if (length != 16) {
    return "";
  }

  const uint8_t *str = (const uint8_t*)s;
  int i = 0;

  char bit_string[kKeyStringLength * 2];

  for (i = 0; i < kKeyStringLength * 2; i += 2) {
      bit_string[i]   = nibble_to_hex_char(*str >> 4);
      bit_string[i + 1] = nibble_to_hex_char(*str++ & 0xF);
  }
  return std::string(bit_string);
}

SrtpChannel::SrtpChannel() {
  boost::mutex::scoped_lock lock(SrtpChannel::sessionMutex_);
  if (SrtpChannel::initialized != true) {
    int res = srtp_init();
    ELOG_DEBUG("Initialized SRTP library %d", res);
    SrtpChannel::initialized = true;
  }

  active_ = false;
  send_session_ = NULL;
  receive_session_ = NULL;
}

SrtpChannel::~SrtpChannel() {
  active_ = false;
  if (send_session_ != NULL) {
    srtp_dealloc(send_session_);
    send_session_ = NULL;
  }
  if (receive_session_ != NULL) {
    srtp_dealloc(receive_session_);
    receive_session_ = NULL;
  }
}

bool SrtpChannel::setRtpParams(const std::string &sendingKey, const std::string &receivingKey) {
  ELOG_DEBUG("Configuring srtp local key %s remote key %s", sendingKey.c_str(), receivingKey.c_str());
  if (configureSrtpSession(&send_session_,    sendingKey,   SENDING) &&
      configureSrtpSession(&receive_session_, receivingKey, RECEIVING)) {
    active_ = true;
    return active_;
  }
  return false;
}

bool SrtpChannel::setRtcpParams(const std::string &sendingKey, const std::string &receivingKey) {
    return 0;
}

int SrtpChannel::protectRtp(char* buffer, int *len) {
  if (!active_) {
    return -1;
  }
  int val = srtp_protect(send_session_, buffer, len);
  if (val == 0) {
    return 0;
  } else {
    RtcpHeader* head = reinterpret_cast<RtcpHeader*>(buffer);
    RtpHeader* headrtp = reinterpret_cast<RtpHeader*>(buffer);

    if (val != 10) {  // Do not warn about reply errors
      ELOG_DEBUG("Error SrtpChannel::protectRtp %u packettype %d pt %d seqnum %u",
                 val, head->packettype, headrtp->payloadtype, headrtp->seqnum);
    }
    return -1;
  }
}

int SrtpChannel::unprotectRtp(char* buffer, int *len) {
  if (!active_) {
    return -1;
  }
  int val = srtp_unprotect(receive_session_, reinterpret_cast<char*>(buffer), len);
  if (val == 0) {
    return 0;
  } else {
    RtcpHeader* head = reinterpret_cast<RtcpHeader*>(buffer);
    RtpHeader* headrtp = reinterpret_cast<RtpHeader*>(buffer);
    if (val != 10) {  // Do not warn about reply errors
      ELOG_DEBUG("Error SrtpChannel::unprotectRtp %u packettype %d pt %d",
                 val, head->packettype, headrtp->payloadtype);
    }
    return -1;
  }
}

int SrtpChannel::protectRtcp(char* buffer, int *len) {
  if (!active_) {
    return -1;
  }
  int val = srtp_protect_rtcp(send_session_, reinterpret_cast<char*>(buffer), len);
  if (val == 0) {
    return 0;
  } else {
    RtcpHeader* head = reinterpret_cast<RtcpHeader*>(buffer);
    if (val != 10) {  // Do not warn about reply errors
      ELOG_DEBUG("Error SrtpChannel::protectRtcp %upackettype %d ", val, head->packettype);
    }
    return -1;
  }
}

int SrtpChannel::unprotectRtcp(char* buffer, int *len) {
  if (!active_) {
    return -1;
  }
  int val = srtp_unprotect_rtcp(receive_session_, buffer, len);
  if (val == 0) {
    return 0;
  } else {
    if (val != 10) {  // Do not warn about reply errors
      ELOG_DEBUG("Error SrtpChannel::unprotectRtcp %u", val);
    }
    return -1;
  }
}

bool SrtpChannel::configureSrtpSession(srtp_t *session, const std::string &key, enum TransmissionType type) {
  srtp_policy_t policy;
  memset(&policy, 0, sizeof(policy));
  srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
  srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
  if (type == SENDING) {
    policy.ssrc.type = ssrc_any_outbound;
  } else {
    policy.ssrc.type = ssrc_any_inbound;
  }

  policy.ssrc.value = 0;
  policy.window_size = 1024;
  policy.allow_repeat_tx = 1;
  policy.next = NULL;
  // ELOG_DEBUG("auth_tag_len %d", policy.rtp.auth_tag_len);

  std::string decoded_key;
  Base64::Decode(key, &decoded_key);
  const std::string::size_type size = decoded_key.size();
  uint8_t *raw_key = new uint8_t[size];
  memcpy(raw_key, decoded_key.c_str(), size);
  ELOG_DEBUG("set master key/salt to %s/", octet_string_hex_string(raw_key, 16).c_str());
  // allocate and initialize the SRTP session
  policy.key = raw_key;
  int res = srtp_create(session, &policy);
  if (res != 0) {
    ELOG_ERROR("Failed to create srtp session with %s, %d", octet_string_hex_string(raw_key, 16).c_str(), res);
  }
  delete[] raw_key;
  return res != 0? false:true;
}

} /*namespace erizo */
