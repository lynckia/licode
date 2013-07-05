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

static const int KEY_LENGTH = 1024;

int createCert (const string& pAor, int expireDays, int keyLen, X509*& outCert, EVP_PKEY*& outKey )
{
   int ret;
   
   cerr << "Generating new user cert for " << pAor << endl;
   string aor = "sip:" + pAor;
   
   // Make sure that necessary algorithms exist:
   assert(EVP_sha1());

   EVP_PKEY* privkey = EVP_PKEY_new();
   assert(privkey);

   RSA* rsa = RSA_new();

   BIGNUM* exponent = BN_new();
   BN_set_word(exponent, 0x10001);

   RSA_generate_key_ex(rsa, KEY_LENGTH, exponent, NULL);

   //RSA* rsa = RSA_generate_key(keyLen, RSA_F4, NULL, NULL);
   assert(rsa);    // couldn't make key pair
   
   ret = EVP_PKEY_set1_RSA(privkey, rsa);
   assert(ret);

   X509* cert = X509_new();
   assert(cert);
   
   X509_NAME* subject = X509_NAME_new();
   X509_EXTENSION* ext = X509_EXTENSION_new();
   
   // set version to X509v3 (starts from 0)
   //X509_set_version(cert, 0L);
   
   int serial = rand();  // get an int worth of randomness
   assert(sizeof(int)==4);
   ASN1_INTEGER_set(X509_get_serialNumber(cert),serial);
   
//    ret = X509_NAME_add_entry_by_txt( subject, "O",  MBSTRING_ASC, 
//                                      (unsigned char *) domain.data(), domain.size(), 
//                                      -1, 0);
   assert(ret);
   ret = X509_NAME_add_entry_by_txt( subject, "CN", MBSTRING_ASC, 
                                     (unsigned char *) aor.data(), aor.size(), 
                                     -1, 0);
   assert(ret);
   
   ret = X509_set_issuer_name(cert, subject);
   assert(ret);
   ret = X509_set_subject_name(cert, subject);
   assert(ret);
   
   const long duration = 60*60*24*expireDays;   
   X509_gmtime_adj(X509_get_notBefore(cert),0);
   X509_gmtime_adj(X509_get_notAfter(cert), duration);
   
   ret = X509_set_pubkey(cert, privkey);
   assert(ret);
   
   string subjectAltNameStr = string("URI:sip:") + aor
      + string(",URI:im:")+aor
      + string(",URI:pres:")+aor;
   ext = X509V3_EXT_conf_nid( NULL , NULL , NID_subject_alt_name, 
                              (char*) subjectAltNameStr.c_str() );
//   X509_add_ext( cert, ext, -1);
   X509_EXTENSION_free(ext);
   
   static char CA_FALSE[] = "CA:FALSE";
   ext = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints, CA_FALSE);
   ret = X509_add_ext( cert, ext, -1);
   assert(ret);
   X509_EXTENSION_free(ext);
   
   // TODO add extensions NID_subject_key_identifier and NID_authority_key_identifier
   
   ret = X509_sign(cert, privkey, EVP_sha1());
   assert(ret);

   outCert = cert;
   outKey = privkey;
   return ret; 
}

//memory is only valid for duration of callback; must be copied if queueing
//is required
DtlsSocketContext::DtlsSocketContext() {
  cout << "Initializing DTLS\n";

  SSL_library_init();
  SSL_load_error_strings();
  ERR_load_crypto_strings();
//  srtp_init();  

  X509 *clientCert;
  EVP_PKEY *clientKey,*serverKey;

  createCert("sip:client@example.com",365,1024,clientCert,clientKey);

  DtlsFactory *clientFactory = new DtlsFactory(std::auto_ptr<DtlsTimerContext>(new TestTimerContext()), clientCert,clientKey);
  
  cout << "Created the factories\n";
  
  mSocket=clientFactory->createClient(std::auto_ptr<DtlsSocketContext>(this));

  cout << "Created the client\n";

  // Assign NICE channel
  
  //clientSocket->startClient();
  
}
     
DtlsSocketContext::~DtlsSocketContext(){
  
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
  cout << "Handling read data len = " << len << endl;
  mSocket->handlePacketMaybe(data, len);
  cout << "Handled" << endl;
}

void DtlsSocketContext::setDtlsReceiver(DtlsReceiver *recv) {
  receiver = recv;
}

void DtlsSocketContext::write(const unsigned char* data, unsigned int len)
{
  cout << ": DTLS Wrapper called write...len = " << len << endl;

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
