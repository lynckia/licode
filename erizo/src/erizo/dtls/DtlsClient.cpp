#include <iostream>
#include <cassert>
#include <string.h>

//#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/bn.h>
#include <openssl/srtp.h>

#include "DtlsSocket.h"
#include "DtlsFactory.h"
#include "DtlsTimer.h"
#include "bf_dwrap.h"

#include <nice/nice.h>

extern "C"
{
#include <srtp/srtp.h>
#include <srtp/srtp_priv.h>
}

using namespace std;
using namespace dtls;

DEFINE_LOGGER(DtlsSocketContext, "dtls.DtlsSocketContext");

//memory is only valid for duration of callback; must be copied if queueing
//is required
DtlsSocketContext::DtlsSocketContext() {
}

DtlsSocketContext::~DtlsSocketContext(){
}

void DtlsSocketContext::stop() {
  delete mSocket;
}

void DtlsSocketContext::setSocket(DtlsSocket *socket) {
  mSocket=socket;
}

std::string DtlsSocketContext::getFingerprint() {
  char fprint[100];
  mSocket->getMyCertFingerprint(fprint);
  return std::string(fprint, strlen(fprint));
}

void DtlsSocketContext::start() {
  mSocket->startClient();
}

void DtlsSocketContext::read(const unsigned char* data, unsigned int len) {
  mSocket->handlePacketMaybe(data, len);
}

void DtlsSocketContext::setDtlsReceiver(DtlsReceiver *recv) {
  receiver = recv;
}

void DtlsSocketContext::write(const unsigned char* data, unsigned int len)
{
  if (receiver != NULL) {
    receiver->writeDtls(this, data, len);
  }
}

void DtlsSocketContext::handshakeCompleted()
{
  char fprint[100];
  SRTP_PROTECTION_PROFILE *srtp_profile;

  if(mSocket->getRemoteFingerprint(fprint)){
    ELOG_TRACE("Remote fingerprint == %s", fprint);

    bool check=mSocket->checkFingerprint(fprint,strlen(fprint));
    ELOG_DEBUG("Fingerprint check == %d", check);

    SrtpSessionKeys keys=mSocket->getSrtpSessionKeys();

    unsigned char* cKey = (unsigned char*)malloc(keys.clientMasterKeyLen + keys.clientMasterSaltLen);
    unsigned char* sKey = (unsigned char*)malloc(keys.serverMasterKeyLen + keys.serverMasterSaltLen);

    memcpy ( cKey, keys.clientMasterKey, keys.clientMasterKeyLen );
    memcpy ( cKey + keys.clientMasterKeyLen, keys.clientMasterSalt, keys.clientMasterSaltLen );

    memcpy ( sKey, keys.serverMasterKey, keys.serverMasterKeyLen );
    memcpy ( sKey + keys.serverMasterKeyLen, keys.serverMasterSalt, keys.serverMasterSaltLen );


    std::string clientKey = g_base64_encode((const guchar*)cKey, keys.clientMasterKeyLen + keys.clientMasterSaltLen);
    std::string serverKey = g_base64_encode((const guchar*)sKey, keys.serverMasterKeyLen + keys.serverMasterSaltLen);

    ELOG_DEBUG("ClientKey: %s", clientKey.c_str());
    ELOG_DEBUG("ServerKey: %s", serverKey.c_str());

    free(cKey);
    free(sKey);

    srtp_profile=mSocket->getSrtpProfile();

    if(srtp_profile){
      ELOG_DEBUG("SRTP Extension negotiated profile=%s", srtp_profile->name);
    }

    if (receiver != NULL) {
      receiver->onHandshakeCompleted(this, clientKey, serverKey, srtp_profile->name);
    }
  }
  else {
    ELOG_DEBUG("Peer did not authenticate");
  }

}

void DtlsSocketContext::handshakeFailed(const char *err)
{
  ELOG_WARN("DTLS Handshake Failure");
}