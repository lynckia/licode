#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>
#include <iostream>
#include "OpenSSLInit.h"

#include <openssl/e_os2.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include "DtlsFactory.h"
#include "DtlsSocket.h"

using namespace dtls;
using namespace std;
const char* DtlsFactory::DefaultSrtpProfile = "SRTP_AES128_CM_SHA1_80";

void
SSLInfoCallback(const SSL* s, int where, int ret) {
  const char* str = "undefined";
  int w = where & ~SSL_ST_MASK;
  if (w & SSL_ST_CONNECT) {
    str = "SSL_connect";
  } else if (w & SSL_ST_ACCEPT) {
    str = "SSL_accept";
  }
  if (where & SSL_CB_LOOP) {
    cout <<  str << ":" << SSL_state_string_long(s) << endl;
  } else if (where & SSL_CB_ALERT) {
    str = (where & SSL_CB_READ) ? "read" : "write";
    cout <<  "SSL3 alert " << str
      << ":" << SSL_alert_type_string_long(ret)
      << ":" << SSL_alert_desc_string_long(ret) << endl;
  } else if (where & SSL_CB_EXIT) {
    if (ret == 0) {
      cout << str << ":failed in " << SSL_state_string_long(s) << endl;
    } else if (ret < 0) {
      cout  << str << ":error in " << SSL_state_string_long(s) << endl;
    }
  }
}

int SSLVerifyCallback(int ok, X509_STORE_CTX* store) {

  if (!ok) {
    char data[256];
    X509* cert = X509_STORE_CTX_get_current_cert(store);
    int depth = X509_STORE_CTX_get_error_depth(store);
    int err = X509_STORE_CTX_get_error(store);

    cout << "Error with certificate at depth: " << depth;
    X509_NAME_oneline(X509_get_issuer_name(cert), data, sizeof(data));
    cout << "  issuer  = " << data << endl;
    X509_NAME_oneline(X509_get_subject_name(cert), data, sizeof(data));
    cout << "  subject = " << data << endl;
    cout << "  err     = " << err
      << ":" << X509_verify_cert_error_string(err) << endl;
  }

  // Get our SSL structure from the store
  SSL* ssl = reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(
                                        store,
                                        SSL_get_ex_data_X509_STORE_CTX_idx()));


  // In peer-to-peer mode, no root cert / certificate authority was
  // specified, so the libraries knows of no certificate to accept,
  // and therefore it will necessarily call here on the first cert it
  // tries to verify.
  if (!ok) {
    X509* cert = X509_STORE_CTX_get_current_cert(store);
    int err = X509_STORE_CTX_get_error(store);

    cout << "Error: " << X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT << endl;

    // peer-to-peer mode: allow the certificate to be self-signed,
    // assuming it matches the digest that was specified.
    if (err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
      unsigned char digest[EVP_MAX_MD_SIZE];
      std::size_t digest_length;

      cout <<
              "Accepted self-signed peer certificate authority" << endl;
      ok = 1;

      /* TODO

      if (OpenSSLCertificate::
         ComputeDigest(cert,
                       stream->peer_certificate_digest_algorithm_,
                       digest, sizeof(digest),
                       &digest_length)) {
        Buffer computed_digest(digest, digest_length);
        if (computed_digest == stream->peer_certificate_digest_value_) {
          cout <<
              "Accepted self-signed peer certificate authority";
          ok = 1;
        }
      }
      */
    }
  } 
  if (!ok) {
    cout << "Ignoring cert error while verifying cert chain" << endl;
    ok = 1;
  }

  return ok;
}

DtlsFactory::DtlsFactory(std::auto_ptr<DtlsTimerContext> tc,X509 *cert, EVP_PKEY *privkey):
   mTimerContext(tc),
   mCert(cert)
{
   int r;
   mContext=SSL_CTX_new(DTLSv1_client_method());
   assert(mContext);

   r=SSL_CTX_use_certificate(mContext, cert);
   assert(r==1);

   r=SSL_CTX_use_PrivateKey(mContext, privkey);
   assert(r==1);

   SSL_CTX_set_cipher_list(mContext, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

   SSL_CTX_set_info_callback(mContext, SSLInfoCallback);

   SSL_CTX_set_verify(mContext, SSL_VERIFY_PEER |SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                     SSLVerifyCallback);

   SSL_CTX_set_session_cache_mode(mContext, SSL_SESS_CACHE_OFF);
   SSL_CTX_set_options(mContext, SSL_OP_NO_TICKET);
   // Set SRTP profiles
   r=SSL_CTX_set_tlsext_use_srtp(mContext, DefaultSrtpProfile);
   assert(r==0);
}

DtlsFactory::~DtlsFactory()
{
   SSL_CTX_free(mContext);
   EVP_MD_CTX_cleanup(ctx_);
}


DtlsSocket*
DtlsFactory::createClient(std::auto_ptr<DtlsSocketContext> context)
{
   return new DtlsSocket(context,this,DtlsSocket::Client);
}

DtlsSocket*
DtlsFactory::createServer(std::auto_ptr<DtlsSocketContext> context)
{
   return new DtlsSocket(context,this,DtlsSocket::Server);  
}

void
DtlsFactory::getMyCertFingerprint(char *fingerprint)
{
   DtlsSocket::computeFingerprint(mCert,fingerprint);
}

void
DtlsFactory::setSrtpProfiles(const char *str)
{
   int r;

   r=SSL_CTX_set_tlsext_use_srtp(mContext,str);

   assert(r==0);
}

void
DtlsFactory::setCipherSuites(const char *str)
{
   int r;

   r=SSL_CTX_set_cipher_list(mContext,str);
   assert(r==1);
}

DtlsFactory::PacketType
DtlsFactory::demuxPacket(const unsigned char *data, unsigned int len) 
{
   assert(len>=1);

   if((data[0]==0)   || (data[0]==1))
      return stun;
   if((data[0]>=128) && (data[0]<=191))
      return rtp;
   if((data[0]>=20)  && (data[0]<=64))
      return dtls;

   return unknown;
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
