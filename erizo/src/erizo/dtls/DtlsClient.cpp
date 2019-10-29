// #include <openssl/x509.h>

extern "C" {
  #include <srtp2/srtp.h>
}
#include <mutex>  // NOLINT
#include <thread>  // NOLINT

#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include <openssl/x509v3.h>

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/bn.h>
#include <openssl/srtp.h>
#include <openssl/opensslv.h>

#include <nice/nice.h>

#include <iostream>
#include <cassert>
#include <string>
#include <cstring>

#include "./DtlsSocket.h"
#include "./bf_dwrap.h"

using dtls::DtlsSocketContext;
using dtls::DtlsSocket;
using std::memcpy;

const char* DtlsSocketContext::DefaultSrtpProfile = "SRTP_AES128_CM_SHA1_80";

X509 *DtlsSocketContext::mCert = nullptr;
EVP_PKEY *DtlsSocketContext::privkey = nullptr;

static const int KEY_LENGTH = 1024;

static std::mutex* array_mutex;

DEFINE_LOGGER(DtlsSocketContext, "dtls.DtlsSocketContext");
log4cxx::LoggerPtr sslLogger(log4cxx::Logger::getLogger("dtls.SSL"));

static void ssl_lock_callback(int mode, int type, const char* file, int line) {
  if (mode & CRYPTO_LOCK) {
    array_mutex[type].lock();
  } else {
    array_mutex[type].unlock();
  }
}

static unsigned long ssl_thread_id() {  // NOLINT
  return (unsigned long)std::hash<std::thread::id>()(std::this_thread::get_id());  // NOLINT
}

static int ssl_thread_setup() {
  array_mutex = new std::mutex[CRYPTO_num_locks()];

  if (!array_mutex) {
    return 0;
  } else {
    CRYPTO_set_id_callback(ssl_thread_id);
    CRYPTO_set_locking_callback(ssl_lock_callback);
  }
  return 1;
}

static int ssl_thread_cleanup() {
  if (!array_mutex) {
    return 0;
  }

  CRYPTO_set_id_callback(nullptr);
  CRYPTO_set_locking_callback(nullptr);
  delete[] array_mutex;
  array_mutex = nullptr;
  return 1;
}

void SSLInfoCallback(const SSL* s, int where, int ret) {
  const char* str = "undefined";
  int w = where & ~SSL_ST_MASK;
  if (w & SSL_ST_CONNECT) {
    str = "SSL_connect";
  } else if (w & SSL_ST_ACCEPT) {
    str = "SSL_accept";
  }
  if (where & SSL_CB_LOOP) {
    ELOG_DEBUG2(sslLogger, "%s", SSL_state_string_long(s));
  } else if (where & SSL_CB_ALERT) {
    str = (where & SSL_CB_READ) ? "read" : "write";
    ELOG_DEBUG2(sslLogger, "SSL3 alert %d - %s; %s : %s",
    ret,
    str,
    SSL_alert_type_string_long(ret),
    SSL_alert_desc_string_long(ret));
  } else if (where & SSL_CB_EXIT) {
    if (ret == 0) {
      ELOG_WARN2(sslLogger, "failed in %s", SSL_state_string_long(s));
    } else if (ret < 0) {
      ELOG_INFO2(sslLogger, "callback for %s", SSL_state_string_long(s));
    }
  }
}

int SSLVerifyCallback(int ok, X509_STORE_CTX* store) {
  if (!ok) {
    char data[256], data2[256];
    X509* cert = X509_STORE_CTX_get_current_cert(store);
    int depth = X509_STORE_CTX_get_error_depth(store);
    int err = X509_STORE_CTX_get_error(store);
    X509_NAME_oneline(X509_get_issuer_name(cert), data, sizeof(data));
    X509_NAME_oneline(X509_get_subject_name(cert), data2, sizeof(data2));
    ELOG_DEBUG2(sslLogger, "Callback with certificate at depth: %d, issuer: %s, subject: %s, err: %d : %s",
    depth,
    data,
    data2,
    err,
    X509_verify_cert_error_string(err));
  }

  // Get our SSL structure from the store
  // SSL* ssl = reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(
  //                                      store,
  //                                      SSL_get_ex_data_X509_STORE_CTX_idx()));


  // In peer-to-peer mode, no root cert / certificate authority was
  // specified, so the libraries knows of no certificate to accept,
  // and therefore it will necessarily call here on the first cert it
  // tries to verify.
  if (!ok) {
    // X509* cert = X509_STORE_CTX_get_current_cert(store);
    int err = X509_STORE_CTX_get_error(store);

    // peer-to-peer mode: allow the certificate to be self-signed,
    // assuming it matches the digest that was specified.
    if (err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
      // unsigned char digest[EVP_MAX_MD_SIZE];
      // std::size_t digest_length;

      ELOG_DEBUG2(sslLogger, "Accepted self-signed peer certificate authority");
      ok = 1;

      /* TODO(javier)

      if (OpenSSLCertificate::ComputeDigest(cert,
            stream->peer_certificate_digest_algorithm_,
            digest, sizeof(digest),
            &digest_length)) {
        Buffer computed_digest(digest, digest_length);
        if (computed_digest == stream->peer_certificate_digest_value_) {
          ELOG_DEBUG2(sslLogger, "Accepted self-signed peer certificate authority");
          ok = 1;
        }
      }
      */
    }
  }
  if (!ok) {
    ELOG_DEBUG2(sslLogger, "Ignoring cert error while verifying cert chain");
    ok = 1;
  }

  return ok;
}

int createCert(const std::string& pAor, int expireDays, int keyLen, X509*& outCert, EVP_PKEY*& outKey) {  // NOLINT
  std::ostringstream info;
  info << "Generating new user cert for" << pAor;
  ELOG_DEBUG2(sslLogger, "%s", info.str().c_str());
  std::string aor = "sip:" + pAor;

  // Make sure that necessary algorithms exist:
  assert(EVP_sha1());

  EVP_PKEY* privkey = EVP_PKEY_new();
  assert(privkey);

  RSA* rsa = RSA_new();

  BIGNUM* exponent = BN_new();
  BN_set_word(exponent, 0x10001);

  RSA_generate_key_ex(rsa, KEY_LENGTH, exponent, NULL);

  // RSA* rsa = RSA_generate_key(keyLen, RSA_F4, NULL, NULL);
  assert(rsa);    // couldn't make key pair

  int ret = EVP_PKEY_set1_RSA(privkey, rsa);
  assert(ret);

  X509* cert = X509_new();
  assert(cert);

  X509_NAME* subject = X509_NAME_new();
  X509_EXTENSION* ext = X509_EXTENSION_new();

  // set version to X509v3 (starts from 0)
  // X509_set_version(cert, 0L);
  std::string thread_id = boost::lexical_cast<std::string>(boost::this_thread::get_id());
  unsigned int thread_number = 0;
  sscanf(thread_id.c_str(), "%x", &thread_number);

  int serial = rand_r(&thread_number);  // get an int worth of randomness
  assert(sizeof(int) == 4);
  ASN1_INTEGER_set(X509_get_serialNumber(cert), serial);

  //    ret = X509_NAME_add_entry_by_txt( subject, "O",  MBSTRING_ASC,
  //                                      (unsigned char *) domain.data(), domain.size(),
  //                                      -1, 0);
  assert(ret);
  ret = X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, (unsigned char *) aor.data(), aor.size(), -1, 0);
  assert(ret);

  ret = X509_set_issuer_name(cert, subject);
  assert(ret);
  ret = X509_set_subject_name(cert, subject);
  assert(ret);

  const long duration = 60 * 60 * 24 * expireDays;  // NOLINT
  X509_gmtime_adj(X509_get_notBefore(cert), 0);
  X509_gmtime_adj(X509_get_notAfter(cert), duration);

  ret = X509_set_pubkey(cert, privkey);
  assert(ret);

  std::string subjectAltNameStr = std::string("URI:sip:") + aor
                                  + std::string(",URI:im:") + aor
                                  + std::string(",URI:pres:") + aor;
  ext = X509V3_EXT_conf_nid(NULL , NULL , NID_subject_alt_name, (char*)subjectAltNameStr.c_str());  // NOLINT
    //   X509_add_ext( cert, ext, -1);
    X509_EXTENSION_free(ext);

    static char CA_FALSE[] = "CA:FALSE";
    ext = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints, CA_FALSE);
    ret = X509_add_ext(cert, ext, -1);
    assert(ret);
    X509_EXTENSION_free(ext);

    // TODO(javier) add extensions NID_subject_key_identifier and NID_authority_key_identifier

    ret = X509_sign(cert, privkey, EVP_sha1());
    assert(ret);

    outCert = cert;
    outKey = privkey;
    return ret;
  }

  // memory is only valid for duration of callback; must be copied if queueing
  // is required
  DtlsSocketContext::DtlsSocketContext() {
    started = false;

    ELOG_DEBUG("Creating Dtls factory, Openssl v %s", OPENSSL_VERSION_TEXT);

    mContext = SSL_CTX_new(DTLS_method());
    assert(mContext);

    int r = SSL_CTX_use_certificate(mContext, mCert);
    assert(r == 1);

    r = SSL_CTX_use_PrivateKey(mContext, privkey);
    assert(r == 1);

    SSL_CTX_set_cipher_list(mContext, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

    SSL_CTX_set_info_callback(mContext, SSLInfoCallback);

    SSL_CTX_set_verify(mContext, SSL_VERIFY_PEER |SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
      SSLVerifyCallback);

    SSL_CTX_set_options(mContext, SSL_OP_NO_QUERY_MTU);
      // SSL_CTX_set_session_cache_mode(mContext, SSL_SESS_CACHE_OFF);
      // SSL_CTX_set_options(mContext, SSL_OP_NO_TICKET);
      // Set SRTP profiles
    r = SSL_CTX_set_tlsext_use_srtp(mContext, DefaultSrtpProfile);
    assert(r == 0);

    SSL_CTX_set_verify_depth(mContext, 2);
    SSL_CTX_set_read_ahead(mContext, 1);

    ELOG_DEBUG("DtlsSocketContext created");
  }

    DtlsSocketContext::~DtlsSocketContext() {
      mSocket->close();
      delete mSocket;
      mSocket = nullptr;
      SSL_CTX_free(mContext);
    }

    void DtlsSocketContext::close() {
      mSocket->close();
    }

    void DtlsSocketContext::Init() {
      ssl_thread_setup();
      if (DtlsSocketContext::mCert == nullptr) {
        OpenSSL_add_all_algorithms();
        SSL_library_init();
        SSL_load_error_strings();
        ERR_load_crypto_strings();
        createCert("sip:licode@lynckia.com", 365, 1024, DtlsSocketContext::mCert, DtlsSocketContext::privkey);
      }
    }

    void DtlsSocketContext::Destroy() {
      ssl_thread_cleanup();
    }

    DtlsSocket* DtlsSocketContext::createClient() {
      return new DtlsSocket(this, DtlsSocket::Client);
    }


    DtlsSocket* DtlsSocketContext::createServer() {
      return new DtlsSocket(this, DtlsSocket::Server);
    }

    void DtlsSocketContext::getMyCertFingerprint(char *fingerprint) {
      DtlsSocket::computeFingerprint(DtlsSocketContext::mCert, fingerprint);
    }

    void DtlsSocketContext::setSrtpProfiles(const char *str) {
      int r = SSL_CTX_set_tlsext_use_srtp(mContext, str);
      assert(r == 0);
    }

    void DtlsSocketContext::setCipherSuites(const char *str) {
      int r = SSL_CTX_set_cipher_list(mContext, str);
      assert(r == 1);
    }

    SSL_CTX* DtlsSocketContext::getSSLContext() {
      return mContext;
    }

    DtlsSocketContext::PacketType DtlsSocketContext::demuxPacket(const unsigned char *data, unsigned int len) {
      assert(len >= 1);

      if ((data[0] == 0)   || (data[0] == 1))
      return stun;
      if ((data[0] >= 128) && (data[0] <= 191))
      return rtp;
      if ((data[0] >= 20)  && (data[0] <= 64))
      return dtls;

      return unknown;
    }


    std::string DtlsSocketContext::getFingerprint() const {
      char fprint[100] = {};
      mSocket->getMyCertFingerprint(fprint);
      return std::string(fprint);
    }

    void DtlsSocketContext::start() {
      started = true;
      mSocket->startClient();
    }

    void DtlsSocketContext::read(const unsigned char* data, unsigned int len) {
      mSocket->handlePacketMaybe(data, len);
    }

    void DtlsSocketContext::setDtlsReceiver(DtlsReceiver *recv) {
      receiver = recv;
    }

    void DtlsSocketContext::write(const unsigned char* data, unsigned int len) {
      if (receiver != NULL) {
        receiver->onDtlsPacket(this, data, len);
      }
    }

    void DtlsSocketContext::handleTimeout() {
      mSocket->handleTimeout();
    }

    void DtlsSocketContext::handshakeCompleted() {
      char fprint[100];
      SRTP_PROTECTION_PROFILE *srtp_profile;

      if (mSocket->getRemoteFingerprint(fprint)) {
        ELOG_TRACE("Remote fingerprint == %s", fprint);

        bool check = mSocket->checkFingerprint(fprint, strlen(fprint));
        ELOG_DEBUG("Fingerprint check == %d", check);

        SrtpSessionKeys* keys = mSocket->getSrtpSessionKeys();

        unsigned char* cKey = (unsigned char*)malloc(keys->clientMasterKeyLen + keys->clientMasterSaltLen);
        unsigned char* sKey = (unsigned char*)malloc(keys->serverMasterKeyLen + keys->serverMasterSaltLen);

        memcpy(cKey, keys->clientMasterKey, keys->clientMasterKeyLen);
        memcpy(cKey + keys->clientMasterKeyLen, keys->clientMasterSalt, keys->clientMasterSaltLen);

        memcpy(sKey, keys->serverMasterKey, keys->serverMasterKeyLen);
        memcpy(sKey + keys->serverMasterKeyLen, keys->serverMasterSalt, keys->serverMasterSaltLen);

        // g_base64_encode must be free'd with g_free.  Also, std::string's assignment operator does *not* take
        // ownership of the passed in ptr; under the hood it copies up to the first null character.
        gchar* temp = g_base64_encode((const guchar*)cKey, keys->clientMasterKeyLen + keys->clientMasterSaltLen);
        std::string clientKey = temp;
        g_free(temp); temp = NULL;

        temp = g_base64_encode((const guchar*)sKey, keys->serverMasterKeyLen + keys->serverMasterSaltLen);
        std::string serverKey = temp;
        g_free(temp); temp = NULL;

        ELOG_DEBUG("ClientKey: %s", clientKey.c_str());
        ELOG_DEBUG("ServerKey: %s", serverKey.c_str());

        free(cKey);
        free(sKey);
        delete keys;

        srtp_profile = mSocket->getSrtpProfile();

        if (srtp_profile) {
          ELOG_DEBUG("SRTP Extension negotiated profile=%s", srtp_profile->name);
        }

        if (receiver != NULL) {
          receiver->onHandshakeCompleted(this, clientKey, serverKey, srtp_profile->name);
        }
      } else {
        ELOG_DEBUG("Peer did not authenticate");
      }
    }

    void DtlsSocketContext::handshakeFailed(const char *err) {
      ELOG_WARN("DTLS Handshake Failure %s", err);
      receiver->onHandshakeFailed(this, std::string(err));
    }
