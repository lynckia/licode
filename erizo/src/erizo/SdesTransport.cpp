/*
 * SdesTransport.cpp
 */
#include <iostream>
#include <cassert>

#include "SdesTransport.h"
#include "NiceConnection.h"
#include "SrtpChannel.h"

#include "rtputils.h"

using namespace erizo;
using namespace std;

SdesTransport::SdesTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, CryptoInfo *remoteCrypto, TransportListener *transportListener):Transport(med, transport_name, bundle, rtcp_mux, transportListener) {
    cout << "Initializing SdesTransport" << endl;
    updateTransportState(TRANSPORT_INITIAL);

    readyRtp = false;
    readyRtcp = false;

    srtp_ = NULL;
    srtcp_ = NULL;
    protectBuf_ =reinterpret_cast<char*>(malloc(10000));
    unprotectBuf_ =reinterpret_cast<char*>(malloc(10000));
    int comps = 1;
    if (!rtcp_mux) {
      comps = 2;
    }
    bundle_ = bundle;

    cryptoLocal_.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
    cryptoLocal_.mediaType = med;
    std::string keyv = SrtpChannel::generateBase64Key();
    printf("Key generated: %s\n", keyv.c_str());
    cryptoLocal_.keyParams = keyv;
    cryptoLocal_.tag = 1;
    cryptoRemote_ = *remoteCrypto;

    nice_ = new NiceConnection(med, transport_name, comps);
    nice_->setNiceListener(this);
    nice_->start();
}

SdesTransport::~SdesTransport() {

  this->close();

  free(protectBuf_);
  free(unprotectBuf_);

}

void SdesTransport::close() {
  if (srtp_ != NULL) {
    free(srtp_);
  }
  if (srtcp_ != NULL) {
    free(srtcp_);
  }
  if (nice_ != NULL) {
     nice_->close();
     nice_->join();
     delete nice_;
  }
}

void SdesTransport::onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice) {
    boost::mutex::scoped_lock lock(readMutex_);
    int length = len;
    SrtpChannel *srtp = srtp_;

    if (this->getTransportState() == TRANSPORT_READY) {
      memset(unprotectBuf_, 0, len);
      memcpy(unprotectBuf_, data, len);

      if (component_id == 2) {
        srtp = srtcp_;
      }

      if (this->getTransportState() == TRANSPORT_READY){
          rtcpheader *chead = reinterpret_cast<rtcpheader*> (unprotectBuf_);
          if (chead->packettype == RTCP_Sender_PT ||
              chead->packettype == RTCP_Receiver_PT ||
              chead->packettype == RTCP_Feedback_PT){
            if(srtp->unprotectRtcp(unprotectBuf_, &length)<0)
              return;
          } else {
            if(srtp->unprotectRtp(unprotectBuf_, &length)<0)
              return;
          }
      } else {
        return;
      }

      if (length <= 0)
          return;

      getTransportListener()->onTransportData(unprotectBuf_, length, this);
    }
}

void SdesTransport::write(char* data, int len) {
   boost::mutex::scoped_lock lock(writeMutex_);
    int length = len;
    SrtpChannel *srtp = srtp_;

    int comp = 1;
    if (this->getTransportState() == TRANSPORT_READY) {
      memset(protectBuf_, 0, len);
      memcpy(protectBuf_, data, len);

      rtcpheader *chead = reinterpret_cast<rtcpheader*> (protectBuf_);
      if (chead->packettype == RTCP_Sender_PT || chead->packettype == RTCP_Receiver_PT || chead->packettype == RTCP_Feedback_PT) {
        if (!rtcp_mux_) {
          comp = 2;
        }
        if (srtcp_ != NULL) {
          srtp = srtcp_;
        }
        if (srtp && nice_->iceState == NICE_READY) {
          if(srtp->protectRtcp(protectBuf_, &length)<0) {
            return;
          }
        }
      }
      else{
        comp = 1;

        if (srtp && nice_->iceState == NICE_READY) {
          if(srtp->protectRtp(protectBuf_, &length)<0) {
            return;
          }
        }
      }
      if (length <= 10) {
        return;
      }
      if (nice_->iceState == NICE_READY) {
          getTransportListener()->queueData(comp, protectBuf_, length, this);
      }
    }
}

void SdesTransport::updateIceState(IceState state, NiceConnection *conn) {
    cout << "New NICE state " << state << " " << mediaType << " " << bundle_ << endl;
    if (state == NICE_CANDIDATES_GATHERED) {
      updateTransportState(TRANSPORT_STARTED);
    }
    if (state == NICE_READY) {
        printf("Setting RTP srtp params local:%s remote:%s\n", cryptoLocal_.keyParams.c_str(), cryptoRemote_.keyParams.c_str());

        srtp_ = new SrtpChannel();
        srtp_->setRtpParams((char*) cryptoLocal_.keyParams.c_str(),
                      (char*) cryptoRemote_.keyParams.c_str());
        if (!rtcp_mux_) {
          printf("Setting RTCP srtp params\n");
          srtcp_ = new SrtpChannel();
          srtp_->setRtpParams((char*) cryptoLocal_.keyParams.c_str(),
                        (char*) cryptoRemote_.keyParams.c_str());
        }
        readyRtp = true;
        readyRtcp = true;
        if (readyRtp && readyRtcp) {
          updateTransportState(TRANSPORT_READY);
        }
    }
}

void SdesTransport::processLocalSdp(SdpInfo *localSdp_) {
  cout << "Processing Local SDP in SDES Transport" << endl;
  std::vector<CandidateInfo> *cands;
  localSdp_->isFingerprint = false;
  localSdp_->addCrypto(cryptoLocal_);
  if (nice_->iceState >= NICE_CANDIDATES_GATHERED) {
    cands = nice_->localCandidates;
    cout << " Candidates: " << cands->size() << endl;
    for (unsigned int it = 0; it < cands->size(); it++) {
      CandidateInfo cand = cands->at(it);
      cand.isBundle = bundle_;
      // TODO Check if bundle
      localSdp_->addCandidate(cand);
      if (cand.isBundle) {
        printf("Adding bundle candidate! %d\n", cand.mediaType);
        cand.mediaType = AUDIO_TYPE;
        localSdp_->addCandidate(cand);
        CryptoInfo temp;
        temp.cipherSuite = cryptoLocal_.cipherSuite;
        temp.mediaType = AUDIO_TYPE;
        temp.keyParams = cryptoLocal_.keyParams;
        temp.tag = 1;
        localSdp_->addCrypto(temp);
      }
    }
  }
  cout << "Processed Local SDP in SDES Transport" << endl;
}