#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef DtlsFactory_h
#define DtlsFactory_h

#include <memory>
#include "DtlsTimer.h"
#include <openssl/evp.h>
typedef struct x509_st X509;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct evp_pkey_st EVP_PKEY;

namespace dtls
{
class DtlsSocket;
class DtlsSocketContext;
class DtlsTimerContext;

//Not threadsafe. Timers must fire in the same thread as dtls processing.
class DtlsFactory
{
   public:
     enum PacketType { rtp, dtls, stun, unknown};
     
     
     

     // Note: this orphans any DtlsSockets you were stupid enough
     // not to free
     ~DtlsFactory();
     
     // Creates a new DtlsSocket to be used as a client
     DtlsSocket* createClient(std::auto_ptr<DtlsSocketContext> context);

     // Creates a new DtlsSocket to be used as a server
     DtlsSocket* createServer(std::auto_ptr<DtlsSocketContext> context);

     // Returns the fingerprint of the user cert that was passed into the constructor
     void getMyCertFingerprint(char *fingerprint);

     // Returns a reference to the timer context that was passed into the constructor
     DtlsTimerContext& getTimerContext() {return *mTimerContext;}

     // The default SrtpProfile used at construction time (default is: SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32)
     static const char* DefaultSrtpProfile; 

     // Changes the default SRTP profiles supported (default is: SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32)
     void setSrtpProfiles(const char *policyStr);

     // Changes the default DTLS Cipher Suites supported
     void setCipherSuites(const char *cipherSuites);

     // Examines the first few bits of a packet to determine its type: rtp, dtls, stun or unknown
     static PacketType demuxPacket(const unsigned char *buf, unsigned int len);

     static DtlsFactory* GetInstance() {
        static DtlsFactory INSTANCE;
        return &INSTANCE;
     }
     
private:
     friend class DtlsSocket;
     // Creates a DTLS SSL Context and enables srtp extension, also sets the private and public key cert
     DtlsFactory();
     SSL_CTX* mContext;
     EVP_MD_CTX* ctx_;
     std::auto_ptr<DtlsTimerContext> mTimerContext;
     X509 *mCert;
};

}

#endif 
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
