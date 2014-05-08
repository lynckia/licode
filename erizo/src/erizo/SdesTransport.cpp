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

DEFINE_LOGGER(SdesTransport, "SdesTransport");

SdesTransport::SdesTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, CryptoInfo *remoteCrypto, TransportListener *transportListener, const std::string &stunServer, int stunPort, int minPort, int maxPort):Transport(med, transport_name, bundle, rtcp_mux, transportListener, stunServer, stunPort, minPort, maxPort) {
    ELOG_DEBUG("Initializing SdesTransport")
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
    //std::string keyv = SrtpChannel::generateBase64Key();
    std::string keyv = "eUMxlV2Ib6U8qeZot/wEKHw9iMzfKUYpOPJrNnu3";
    ELOG_DEBUG("Key generated: %s", keyv.c_str());
    cryptoLocal_.keyParams = keyv;
    cryptoLocal_.tag = 1;
    cryptoRemote_ = *remoteCrypto;

    nice_.reset(new NiceConnection(med, transport_name, comps,  stunServer, stunPort, minPort, maxPort));
    nice_->setNiceListener(this);
    nice_->start();
}

SdesTransport::~SdesTransport() {
    if (srtp_ != NULL) {
        delete srtp_; srtp_ = NULL;
    }
    if (srtcp_ != NULL) {
        delete srtcp_; srtcp_ = NULL;
    }
    if (protectBuf_ != NULL) {
        free(protectBuf_); protectBuf_ = NULL;
    }
    if (unprotectBuf_ != NULL) {
        free(unprotectBuf_); unprotectBuf_ = NULL;
    }
}

void SdesTransport::onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice) {
    //boost::mutex::scoped_lock lock(readMutex_);
    int length = len;
    SrtpChannel *srtp = srtp_;

    if (this->getTransportState() == TRANSPORT_READY) {
      memcpy(unprotectBuf_, data, len);

      if (component_id == 2) {
        srtp = srtcp_;
      }

      rtcpheader *chead = reinterpret_cast<rtcpheader*> (unprotectBuf_);
      if (chead->isRtcp()){
        if(srtp->unprotectRtcp(unprotectBuf_, &length)<0)
          return;
      } else {
        if(srtp->unprotectRtp(unprotectBuf_, &length)<0)
          return;
      }

      if (length <= 0)
          return;

      getTransportListener()->onTransportData(unprotectBuf_, length, this);
    }
}

void SdesTransport::write(char* data, int len) {
   //boost::mutex::scoped_lock lock(writeMutex_);
    int length = len;
    SrtpChannel *srtp = srtp_;

    int comp = 1;
    if (this->getTransportState() == TRANSPORT_READY) {
      memcpy(protectBuf_, data, len);

      rtcpheader *chead = reinterpret_cast<rtcpheader*> (protectBuf_);
      if (chead->packettype == RTCP_Sender_PT || chead->packettype == RTCP_Receiver_PT || chead->packettype == RTCP_PS_Feedback_PT
          || chead->packettype == RTCP_RTP_Feedback_PT) {
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
         ELOG_DEBUG( "New NICE state %d %d %d" , state , mediaType , bundle_ );
    if (state == NICE_CANDIDATES_GATHERED) {
      updateTransportState(TRANSPORT_STARTED);
    }
    if (state == NICE_READY && !readyRtp) {
        ELOG_DEBUG("Setting RTP srtp params local:%s remote:%s", cryptoLocal_.keyParams.c_str(), cryptoRemote_.keyParams.c_str());

        srtp_ = new SrtpChannel();
        srtp_->setRtpParams((char*) cryptoLocal_.keyParams.c_str(),
                      (char*) cryptoRemote_.keyParams.c_str());
        if (!rtcp_mux_) {
          ELOG_DEBUG("Setting RTCP srtp params");
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
  ELOG_DEBUG( "Processing Local SDP in SDES Transport" );
  bool printedAudio = false;
  localSdp_->isFingerprint = false;
  localSdp_->addCrypto(cryptoLocal_);
  if (nice_->iceState >= NICE_CANDIDATES_GATHERED) {
    boost::shared_ptr<std::vector<CandidateInfo> > cands = nice_->localCandidates;
    ELOG_DEBUG( " Candidates: %lu" , cands->size() );
    for (unsigned int it = 0; it < cands->size(); it++) {
      CandidateInfo cand = cands->at(it);
      cand.isBundle = bundle_;
      // TODO Check if bundle
      localSdp_->addCandidate(cand);
      if (cand.isBundle) {
        ELOG_DEBUG("Adding bundle candidate! %d", cand.mediaType);
        cand.mediaType = AUDIO_TYPE;
        localSdp_->addCandidate(cand);
        if (!printedAudio) {
          CryptoInfo temp;
          temp.cipherSuite = cryptoLocal_.cipherSuite;
          temp.mediaType = AUDIO_TYPE;
          temp.keyParams = cryptoLocal_.keyParams;
          temp.tag = 1;
          localSdp_->addCrypto(temp);
          printedAudio = true;
        }
      }
    }
  }
  ELOG_DEBUG( "Processed Local SDP in SDES Transport" );
}
