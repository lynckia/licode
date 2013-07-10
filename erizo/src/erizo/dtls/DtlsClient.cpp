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
    receiver->writeDtls(data, len);
  }
}

void DtlsSocketContext::handshakeCompleted()
{
  char fprint[100];
  SRTP_PROTECTION_PROFILE *srtp_profile;

  cout << ": Hey, amazing, it worked\n";

  if(mSocket->getRemoteFingerprint(fprint)){
    cout <<  ": Remote fingerprint == " << fprint << endl;

    bool check=mSocket->checkFingerprint(fprint,strlen(fprint));

    cout <<  ": Fingerprint check == " << check << endl;

    SrtpSessionKeys keys=mSocket->getSrtpSessionKeys();

    cout << octet_string_hex_string(keys.clientMasterKey, keys.clientMasterKeyLen)
         << "/";
    cout << octet_string_hex_string(keys.clientMasterSalt, keys.clientMasterSaltLen) << endl;
    cout << octet_string_hex_string(keys.serverMasterKey, keys.serverMasterKeyLen)
         << "/";
    cout << octet_string_hex_string(keys.serverMasterSalt, keys.serverMasterSaltLen)
         << endl;

    unsigned char* cKey = (unsigned char*)malloc(keys.clientMasterKeyLen + keys.clientMasterSaltLen);
    unsigned char* sKey = (unsigned char*)malloc(keys.serverMasterKeyLen + keys.serverMasterSaltLen);

    memcpy ( cKey, keys.clientMasterKey, keys.clientMasterKeyLen );
    memcpy ( cKey + keys.clientMasterKeyLen, keys.clientMasterSalt, keys.clientMasterSaltLen );

    memcpy ( sKey, keys.serverMasterKey, keys.serverMasterKeyLen );
    memcpy ( sKey + keys.serverMasterKeyLen, keys.serverMasterSalt, keys.serverMasterSaltLen );

    cout << octet_string_hex_string(cKey, keys.clientMasterKeyLen + keys.clientMasterSaltLen) << endl;

    std::string clientKey = g_base64_encode((const guchar*)cKey, keys.clientMasterKeyLen + keys.clientMasterSaltLen);
    std::string serverKey = g_base64_encode((const guchar*)sKey, keys.serverMasterKeyLen + keys.serverMasterSaltLen);


    cout << "ClientKey: " << clientKey << endl;
    cout << "ServerKey: " << serverKey << endl;

    free(cKey);
    free(sKey);

    srtp_profile=mSocket->getSrtpProfile();

    if(srtp_profile){
      cout <<  ": SRTP Extension negotiated profile="<<srtp_profile->name << endl;
    }

    if (receiver != NULL) {
      receiver->onHandshakeCompleted(clientKey, serverKey, srtp_profile->name);
    }
  }
  else {
    cout <<  ": Peer did not authenticate" << endl;
  }

}

void DtlsSocketContext::handshakeFailed(const char *err)
{
  cout <<  "Bummer, handshake failure "<<err<<endl;
}
