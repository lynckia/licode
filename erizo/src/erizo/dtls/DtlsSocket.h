#ifndef ERIZO_SRC_ERIZO_DTLS_DTLSSOCKET_H_
#define ERIZO_SRC_ERIZO_DTLS_DTLSSOCKET_H_

extern "C" {
  #include <srtp2/srtp.h>
}

#include <openssl/e_os2.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>

#include <memory>
#include <string>

#include "dtls/bf_dwrap.h"
#include "../logger.h"

const int SRTP_MASTER_KEY_KEY_LEN = 16;
const int SRTP_MASTER_KEY_SALT_LEN = 14;
static const int DTLS_MTU = 1472;

namespace dtls {
class DtlsSocketContext;

class SrtpSessionKeys {
 public:
  SrtpSessionKeys() {
    clientMasterKey = new unsigned char[SRTP_MASTER_KEY_KEY_LEN];
    clientMasterKeyLen = 0;
    clientMasterSalt = new unsigned char[SRTP_MASTER_KEY_SALT_LEN];
    clientMasterSaltLen = 0;
    serverMasterKey = new unsigned char[SRTP_MASTER_KEY_KEY_LEN];
    serverMasterKeyLen = 0;
    serverMasterSalt = new unsigned char[SRTP_MASTER_KEY_SALT_LEN];
    serverMasterSaltLen = 0;
  }
  ~SrtpSessionKeys() {
    if (clientMasterKey) {
      delete[] clientMasterKey; clientMasterKey = NULL;
    }
    if (serverMasterKey) {
      delete[] serverMasterKey; serverMasterKey = NULL;
    }
    if (clientMasterSalt) {
      delete[] clientMasterSalt; clientMasterSalt = NULL;
    }
    if (serverMasterSalt) {
      delete[] serverMasterSalt; serverMasterSalt = NULL;
    }
  }
  unsigned char *clientMasterKey;
  int clientMasterKeyLen;
  unsigned char *serverMasterKey;
  int serverMasterKeyLen;
  unsigned char *clientMasterSalt;
  int clientMasterSaltLen;
  unsigned char *serverMasterSalt;
  int serverMasterSaltLen;
};

class DtlsSocket {
  DECLARE_LOGGER();

 public:
  enum SocketType { Client, Server};
  // Creates an SSL socket, and if client sets state to connect_state and
  // if server sets state to accept_state.  Sets SSL BIO's.
  DtlsSocket(DtlsSocketContext* socketContext, enum SocketType type);
  ~DtlsSocket();

  void close();

  // Inspects packet to see if it's a DTLS packet, if so continue processing
  bool handlePacketMaybe(const unsigned char* bytes, unsigned int len);

  // Retrieves the finger print of the certificate presented by the remote party
  bool getRemoteFingerprint(char *fingerprint);

  // Retrieves the finger print of the certificate presented by the remote party and checks
  // it agains the passed in certificate
  bool checkFingerprint(const char* fingerprint, unsigned int len);

  // Retrieves the finger print of our local certificate, same as getMyCertFingerprint
  void getMyCertFingerprint(char *fingerprint);

  // For client sockets only - causes a client handshake to start (doHandshakeIteration)
  void startClient();

  // Retreives the SRTP session keys from the Dtls session
  SrtpSessionKeys* getSrtpSessionKeys();

  // Utility fn to compute a certificates fingerprint
  static void computeFingerprint(X509 *cert, char *fingerprint);

  // Retrieves the DTLS negotiated SRTP profile - may return 0 if profile selection failed
  SRTP_PROTECTION_PROFILE* getSrtpProfile();

  // Creates SRTP session policies appropriately based on socket type (client vs server) and keys
  // extracted from the DTLS handshake process
  void createSrtpSessionPolicies(srtp_policy_t& outboundPolicy, srtp_policy_t& inboundPolicy);  // NOLINT

  void handleTimeout();

 private:
  // Causes an immediate handshake iteration to happen, which will retransmit the handshake
  void forceRetransmit();


  // Give CPU cyces to the handshake process - checks current state and acts appropraitely
  void doHandshakeIteration();

  // Internals
  DtlsSocketContext* mSocketContext;

  // OpenSSL context data
  BIO_METHOD *dwrap_bio_method;
  SSL *mSsl;
  BIO *mInBio;
  BIO *mOutBio;

  SocketType mSocketType;
  bool mHandshakeCompleted;
  boost::mutex handshakeMutex_;
};

class DtlsReceiver {
 public:
  virtual void onDtlsPacket(DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) = 0;
  virtual void onHandshakeCompleted(DtlsSocketContext *ctx, std::string clientKey, std::string serverKey,
                                    std::string srtp_profile) = 0;
  virtual void onHandshakeFailed(DtlsSocketContext *ctx, const std::string& error) = 0;
};

class DtlsSocketContext {
  DECLARE_LOGGER();

 public:
  bool started;
  // memory is only valid for duration of callback; must be copied if queueing
  // is required
  DtlsSocketContext();
  virtual ~DtlsSocketContext();

  void close();

  void start();
  void read(const unsigned char* data, unsigned int len);
  void write(const unsigned char* data, unsigned int len);
  void handshakeCompleted();
  void handshakeFailed(const char *err);
  void setDtlsReceiver(DtlsReceiver *recv);
  void setDtlsSocket(DtlsSocket *sock) {mSocket = sock;}
  std::string getFingerprint() const;

  void handleTimeout();

  enum PacketType { rtp, dtls, stun, unknown};


  // Creates a new DtlsSocket to be used as a client
  DtlsSocket* createClient();

  // Creates a new DtlsSocket to be used as a server
  DtlsSocket* createServer();

  // Returns the fingerprint of the user cert that was passed into the constructor
  void getMyCertFingerprint(char *fingerprint);

  // The default SrtpProfile used at construction time (default is: SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32)
  static const char* DefaultSrtpProfile;

  // Changes the default SRTP profiles supported (default is: SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32)
  void setSrtpProfiles(const char *policyStr);

  // Changes the default DTLS Cipher Suites supported
  void setCipherSuites(const char *cipherSuites);

  SSL_CTX* getSSLContext();

  // Examines the first few bits of a packet to determine its type: rtp, dtls, stun or unknown
  static PacketType demuxPacket(const unsigned char *buf, unsigned int len);

  static X509 *mCert;
  static EVP_PKEY *privkey;

  static void Init();
  static void Destroy();

 protected:
  DtlsSocket *mSocket;
  DtlsReceiver *receiver;

 private:
  // Creates a DTLS SSL Context and enables srtp extension, also sets the private and public key cert
  SSL_CTX* mContext;
};
}  // namespace dtls

#endif  // ERIZO_SRC_ERIZO_DTLS_DTLSSOCKET_H_
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
