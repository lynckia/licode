/*
 * Srtpchannel.cpp
 */

#include <srtp/srtp.h>
#include <nice/nice.h>

#include "SrtpChannel.h"



namespace erizo {
    DEFINE_LOGGER(SrtpChannel, "SrtpChannel");
    bool SrtpChannel::initialized = false;
    boost::mutex SrtpChannel::sessionMutex_;

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

bool SrtpChannel::setRtpParams(char* sendingKey, char* receivingKey) {
    ELOG_DEBUG("Configuring srtp local key %s remote key %s", sendingKey,
            receivingKey);
  if (configureSrtpSession(&send_session_, sendingKey, SENDING) && configureSrtpSession(&receive_session_, receivingKey, RECEIVING)){
    active_ = true;
    return active_;
  }
  return false; 
}

bool SrtpChannel::setRtcpParams(char* sendingKey, char* receivingKey) {

    return 0;
}

int SrtpChannel::protectRtp(char* buffer, int *len) {

    if (!active_)
        return 0;
    int val = srtp_protect(send_session_, buffer, len);
    if (val == 0) {
        return 0;
    } else {
        RtcpHeader* head = reinterpret_cast<RtcpHeader*>(buffer);
        RtpHeader* headrtp = reinterpret_cast<RtpHeader*>(buffer);

        ELOG_WARN("Error SrtpChannel::protectRtp %u packettype %d pt %d seqnum %u", val,head->packettype, headrtp->payloadtype, headrtp->seqnum);
        return -1;
    }
}

int SrtpChannel::unprotectRtp(char* buffer, int *len) {

    if (!active_)
        return 0;
    int val = srtp_unprotect(receive_session_, (char*) buffer, len);
    if (val == 0) {
        return 0;
    } else {
      RtcpHeader* head = reinterpret_cast<RtcpHeader*>(buffer);
      RtpHeader* headrtp = reinterpret_cast<RtpHeader*>(buffer);
      ELOG_WARN("Error SrtpChannel::unprotectRtp %u packettype %d pt %d", val,head->packettype, headrtp->payloadtype);
      return -1;
    }
}

int SrtpChannel::protectRtcp(char* buffer, int *len) {

    int val = srtp_protect_rtcp(send_session_, (char*) buffer, len);
    if (val == 0) {
        return 0;
    } else {
        RtcpHeader* head = reinterpret_cast<RtcpHeader*>(buffer);
        ELOG_WARN("Error SrtpChannel::protectRtcp %upackettype %d ", val, head->packettype);
        return -1;
    }
}

int SrtpChannel::unprotectRtcp(char* buffer, int *len) {

    int val = srtp_unprotect_rtcp(receive_session_, buffer, len);
    if (val == 0) {
        return 0;
    } else {
        ELOG_WARN("Error SrtpChannel::unprotectRtcp %u", val);
        return -1;
    }
}

std::string SrtpChannel::generateBase64Key() {

    unsigned char key[30];
    crypto_get_random(key, 30);
    gchar* base64key = g_base64_encode((guchar*) key, 30);
    return std::string(base64key);
}

bool SrtpChannel::configureSrtpSession(srtp_t *session, const char* key,
        enum TransmissionType type) {
    srtp_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
    crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
    if (type == SENDING) {
        policy.ssrc.type = ssrc_any_outbound;
    } else {

        policy.ssrc.type = ssrc_any_inbound;
    }

    policy.ssrc.value = 0;
    policy.window_size = 1024;
    policy.allow_repeat_tx = 1;
    policy.next = NULL;
    //ELOG_DEBUG("auth_tag_len %d", policy.rtp.auth_tag_len);

    gsize len = 0;
    uint8_t *akey = (uint8_t*) g_base64_decode((gchar*) key, &len);
    ELOG_DEBUG("set master key/salt to %s/", octet_string_hex_string(akey, 16));
    // allocate and initialize the SRTP session
    policy.key = akey;
    int res = srtp_create(session, &policy);
    if (res!=0){
      ELOG_ERROR("Failed to create srtp session with %s, %d", octet_string_hex_string(akey, 16), res);
    }
    return res!=0? false:true;
}

} /*namespace erizo */

