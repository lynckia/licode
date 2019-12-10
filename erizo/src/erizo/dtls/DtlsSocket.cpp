#include "dtls/DtlsSocket.h"

#include <iostream>
#include <cassert>
#include <cstring>
#include <string>

#ifdef HAVE_CONFIG_H
#include "./config.h"
#endif

using dtls::DtlsSocket;
using dtls::SrtpSessionKeys;
using std::memcpy;

DEFINE_LOGGER(DtlsSocket, "dtls.DtlsSocket");

int dummy_cb(int d, X509_STORE_CTX *x) {
  return 1;
}

DtlsSocket::DtlsSocket(DtlsSocketContext* socketContext, enum SocketType type):
              mSocketContext(socketContext),
              mSocketType(type),
              mHandshakeCompleted(false) {
  ELOG_DEBUG("Creating Dtls Socket");
  mSocketContext->setDtlsSocket(this);
  SSL_CTX* mContext = mSocketContext->getSSLContext();
  assert(mContext);
  mSsl = SSL_new(mContext);
  assert(mSsl != 0);
  SSL_set_mtu(mSsl, DTLS_MTU);

  switch (type) {
    case Client:
      SSL_set_connect_state(mSsl);
      // SSL_set_mode(mSsl, SSL_MODE_ENABLE_PARTIAL_WRITE |
      //         SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
      break;
    case Server:
      SSL_set_accept_state(mSsl);
      SSL_set_verify(mSsl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, dummy_cb);
      break;
    default:
      assert(0);
  }

  dwrap_bio_method = BIO_f_dwrap();

  BIO* mem_in_BIO = BIO_new(BIO_s_mem());
  mInBio = BIO_new(dwrap_bio_method);
  BIO_push(mInBio, mem_in_BIO);

  BIO* mem_out_BIO = BIO_new(BIO_s_mem());
  mOutBio = BIO_new(dwrap_bio_method);
  BIO_push(mOutBio, mem_out_BIO);

  SSL_set_bio(mSsl, mInBio, mOutBio);
  SSL_accept(mSsl);
  ELOG_DEBUG("Dtls Socket created");
}

DtlsSocket::~DtlsSocket() {
  close();
}

void DtlsSocket::close() {
  // Properly shutdown the socket and free it - note: this also free's the BIO's
  if (mSsl != NULL) {
    ELOG_DEBUG("SSL Shutdown");
    SSL_shutdown(mSsl);
    SSL_free(mSsl);
    BIO_f_wrap_destroy(dwrap_bio_method);
    mSsl = NULL;
  }
}

void DtlsSocket::startClient() {
  assert(mSocketType == Client);
  doHandshakeIteration();
}

bool DtlsSocket::handlePacketMaybe(const unsigned char* bytes, unsigned int len) {
  if (mSsl == NULL) {
    ELOG_WARN("handlePacketMaybe called after DtlsSocket closed: %p", this);
    return false;
  }
  DtlsSocketContext::PacketType pType = DtlsSocketContext::demuxPacket(bytes, len);

  if (pType != DtlsSocketContext::dtls) {
    return false;
  }

  if (mSsl == nullptr) {
    return false;
  }

  (void) BIO_reset(mInBio);
  (void) BIO_reset(mOutBio);

  int r = BIO_write(mInBio, bytes, len);
  assert(r == static_cast<int>(len));  // Can't happen

  // Note: we must catch any below exceptions--if there are any
  try {
    doHandshakeIteration();
  } catch (int e) {
    return false;
  }
  return true;
}

void DtlsSocket::forceRetransmit() {
  (void) BIO_reset(mInBio);
  (void) BIO_reset(mOutBio);
  BIO_ctrl(mInBio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, 0);

  doHandshakeIteration();
}

void DtlsSocket::doHandshakeIteration() {
  boost::mutex::scoped_lock lock(handshakeMutex_);
  int ssl_error_code;

  if (mHandshakeCompleted) {
    return;
  }

  int return_value = SSL_do_handshake(mSsl);

  // See what was written
  unsigned char *outBioData;
  int outBioLen = BIO_get_mem_data(mOutBio, &outBioData);
  if (outBioLen > DTLS_MTU) {
    ELOG_WARN("message: BIO data bigger than MTU - packet could be lost, outBioLen %u, MTU %u",
        outBioLen, DTLS_MTU);
  }

  // Now handle handshake errors */
  switch (ssl_error_code = SSL_get_error(mSsl, return_value)) {
    case SSL_ERROR_NONE:
      mHandshakeCompleted = true;
      mSocketContext->handshakeCompleted();
      break;
    case SSL_ERROR_WANT_READ:
      break;
    default:
      ELOG_ERROR("message: SSL error %d", ssl_error_code);
      char error_string_buffer[1024];

      ERR_error_string_n(ssl_error_code, error_string_buffer, sizeof(error_string_buffer));

      mSocketContext->handshakeFailed(error_string_buffer);
      // Note: need to fall through to propagate alerts, if any
      break;
  }

  // If mOutBio is now nonzero-length, then we need to write the
  // data to the network. TODO(pedro): warning, MTU issues!
  if (outBioLen) {
    mSocketContext->write(outBioData, outBioLen);
  }
}

bool DtlsSocket::getRemoteFingerprint(char *fprint) {
  X509* x = SSL_get_peer_certificate(mSsl);
  if (!x) {  // No certificate
    return false;
  }

  computeFingerprint(x, fprint);
  X509_free(x);
  return true;
}

bool DtlsSocket::checkFingerprint(const char* fingerprint, unsigned int len) {
  char fprint[100];

  if (getRemoteFingerprint(fprint) == false) {
    return false;
  }

  // used to be strncasecmp
  if (strncmp(fprint, fingerprint, len)) {
    ELOG_WARN("Fingerprint mismatch, got %s expecting %s", fprint, fingerprint);
    return false;
  }

  return true;
}

void DtlsSocket::getMyCertFingerprint(char *fingerprint) {
  mSocketContext->getMyCertFingerprint(fingerprint);
}

SrtpSessionKeys* DtlsSocket::getSrtpSessionKeys() {
  // TODO(pedro): probably an exception candidate
  assert(mHandshakeCompleted);

  SrtpSessionKeys* keys = new SrtpSessionKeys();

  unsigned char material[SRTP_MASTER_KEY_LEN << 1];
  if (!SSL_export_keying_material(mSsl, material, sizeof(material), "EXTRACTOR-dtls_srtp", 19, NULL, 0, 0)) {
    return keys;
  }

  size_t offset = 0;

  memcpy(keys->clientMasterKey, &material[offset], SRTP_MASTER_KEY_KEY_LEN);
  offset += SRTP_MASTER_KEY_KEY_LEN;
  memcpy(keys->serverMasterKey, &material[offset], SRTP_MASTER_KEY_KEY_LEN);
  offset += SRTP_MASTER_KEY_KEY_LEN;
  memcpy(keys->clientMasterSalt, &material[offset], SRTP_MASTER_KEY_SALT_LEN);
  offset += SRTP_MASTER_KEY_SALT_LEN;
  memcpy(keys->serverMasterSalt, &material[offset], SRTP_MASTER_KEY_SALT_LEN);
  offset += SRTP_MASTER_KEY_SALT_LEN;
  keys->clientMasterKeyLen = SRTP_MASTER_KEY_KEY_LEN;
  keys->serverMasterKeyLen = SRTP_MASTER_KEY_KEY_LEN;
  keys->clientMasterSaltLen = SRTP_MASTER_KEY_SALT_LEN;
  keys->serverMasterSaltLen = SRTP_MASTER_KEY_SALT_LEN;

  return keys;
}

SRTP_PROTECTION_PROFILE* DtlsSocket::getSrtpProfile() {
  // TODO(pedro): probably an exception candidate
  assert(mHandshakeCompleted);
  return SSL_get_selected_srtp_profile(mSsl);
}

// Fingerprint is assumed to be long enough
void DtlsSocket::computeFingerprint(X509 *cert, char *fingerprint) {
  unsigned char md[EVP_MAX_MD_SIZE];
  int r;
  unsigned int i, n;

  // r = X509_digest(cert, EVP_sha1(), md, &n);
  r = X509_digest(cert, EVP_sha256(), md, &n);
  // TODO(javier) - is sha1 vs sha256 supposed to come from DTLS handshake?
  // fixing to to SHA-256 for compatibility with current web-rtc implementations
  assert(r == 1);

  for (i = 0; i < n; i++) {
    sprintf(fingerprint, "%02X", md[i]);  // NOLINT
    fingerprint += 2;

    if (i < (n-1))
    *fingerprint++ = ':';
    else
    *fingerprint++ = 0;
  }
}

void DtlsSocket::handleTimeout() {
  (void) BIO_reset(mInBio);
  (void) BIO_reset(mOutBio);
  if (DTLSv1_handle_timeout(mSsl) > 0) {
    ELOG_DEBUG("Dtls timeout occurred!");

    // See what was written
    unsigned char *outBioData;
    int outBioLen = BIO_get_mem_data(mOutBio, &outBioData);
    if (outBioLen > DTLS_MTU) {
      ELOG_WARN("message: BIO data bigger than MTU - packet could be lost, outBioLen %u, MTU %u",
          outBioLen, DTLS_MTU);
    }

    // If mOutBio is now nonzero-length, then we need to write the
    // data to the network. TODO(pedro): warning, MTU issues!
    if (outBioLen) {
      mSocketContext->write(outBioData, outBioLen);
    }
  }
}

// TODO(pedro): assert(0) into exception, as elsewhere
void DtlsSocket::createSrtpSessionPolicies(srtp_policy_t& outboundPolicy, srtp_policy_t& inboundPolicy) {
  assert(mHandshakeCompleted);

  /* we assume that the default profile is in effect, for now */
  srtp_profile_t profile = srtp_profile_aes128_cm_sha1_80;
  int key_len = srtp_profile_get_master_key_length(profile);
  int salt_len = srtp_profile_get_master_salt_length(profile);

  /* get keys from srtp_key and initialize the inbound and outbound sessions */
  uint8_t *client_master_key_and_salt = new uint8_t[SRTP_MAX_KEY_LEN];
  uint8_t *server_master_key_and_salt = new uint8_t[SRTP_MAX_KEY_LEN];
  srtp_policy_t client_policy;
  memset(&client_policy, 0, sizeof(srtp_policy_t));
  client_policy.window_size = 128;
  client_policy.allow_repeat_tx = 1;
  srtp_policy_t server_policy;
  memset(&server_policy, 0, sizeof(srtp_policy_t));
  server_policy.window_size = 128;
  server_policy.allow_repeat_tx = 1;

  SrtpSessionKeys* srtp_key = getSrtpSessionKeys();
  /* set client_write key */
  client_policy.key = client_master_key_and_salt;
  if (srtp_key->clientMasterKeyLen != key_len) {
    ELOG_WARN("error: unexpected client key length");
    assert(0);
  }
  if (srtp_key->clientMasterSaltLen != salt_len) {
    ELOG_WARN("error: unexpected client salt length");
    assert(0);
  }

  memcpy(client_master_key_and_salt, srtp_key->clientMasterKey, key_len);
  memcpy(client_master_key_and_salt + key_len, srtp_key->clientMasterSalt, salt_len);

  /* initialize client SRTP policy from profile  */
  srtp_err_status_t err = srtp_crypto_policy_set_from_profile_for_rtp(&client_policy.rtp, profile);
  if (err) assert(0);

  err = srtp_crypto_policy_set_from_profile_for_rtcp(&client_policy.rtcp, profile);
  if (err) assert(0);
  client_policy.next = NULL;

  /* set server_write key */
  server_policy.key = server_master_key_and_salt;

  if (srtp_key->serverMasterKeyLen != key_len) {
    ELOG_WARN("error: unexpected server key length");
    assert(0);
  }
  if (srtp_key->serverMasterSaltLen != salt_len) {
    ELOG_WARN("error: unexpected salt length");
    assert(0);
  }

  memcpy(server_master_key_and_salt, srtp_key->serverMasterKey, key_len);
  memcpy(server_master_key_and_salt + key_len, srtp_key->serverMasterSalt, salt_len);

  delete srtp_key;

  /* initialize server SRTP policy from profile  */
  err = srtp_crypto_policy_set_from_profile_for_rtp(&server_policy.rtp, profile);
  if (err) assert(0);

  err = srtp_crypto_policy_set_from_profile_for_rtcp(&server_policy.rtcp, profile);
  if (err) assert(0);
  server_policy.next = NULL;

  if (mSocketType == Client) {
    client_policy.ssrc.type = ssrc_any_outbound;
    outboundPolicy = client_policy;

    server_policy.ssrc.type  = ssrc_any_inbound;
    inboundPolicy = server_policy;
  } else {
    server_policy.ssrc.type  = ssrc_any_outbound;
    outboundPolicy = server_policy;

    client_policy.ssrc.type = ssrc_any_inbound;
    inboundPolicy = client_policy;
  }
  /* zeroize the input keys (but not the srtp session keys that are in use) */
  // not done...not much of a security whole imho...the lifetime of these seems odd though
  //    memset(client_master_key_and_salt, 0x00, SRTP_MAX_KEY_LEN);
  //    memset(server_master_key_and_salt, 0x00, SRTP_MAX_KEY_LEN);
  //    memset(&srtp_key, 0x00, sizeof(srtp_key));
}

  /* ====================================================================

  Copyright (c) 2007-2008, Eric Rescorla and Derek MacDonald
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  3. None of the contributors names may be used to endorse or promote
  products derived from this software without specific prior written
  permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  ==================================================================== */
