/*
 * DtlsConnection.cpp
 */
#include <iostream>
#include <cassert>

#include "DtlsTransport.h"
#include "NiceConnection.h"
#include "SrtpChannel.h"

#include "dtls/DtlsFactory.h"

#include "rtputils.h"

using namespace erizo;
using namespace std;
using namespace dtls;

DEFINE_LOGGER(DtlsTransport, "DtlsTransport");

DtlsTransport::DtlsTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, TransportListener *transportListener, const std::string &stunServer, int stunPort, int minPort, int maxPort):Transport(med, transport_name, bundle, rtcp_mux, transportListener, stunServer, stunPort, minPort, maxPort) {
  ELOG_DEBUG( "Initializing DtlsTransport" );
  updateTransportState(TRANSPORT_INITIAL);

  readyRtp = false;
  readyRtcp = false;

  dtlsRtp = new DtlsSocketContext();
  dtlsRtcp = NULL;

  DtlsSocket *mSocket=(new DtlsFactory())->createClient(std::auto_ptr<DtlsSocketContext>(dtlsRtp));
  dtlsRtp->setSocket(mSocket);
  dtlsRtp->setDtlsReceiver(this);

  srtp_ = NULL;
  srtcp_ = NULL;
  int comps = 1;
  if (!rtcp_mux) {
    comps = 2;
    dtlsRtcp = new DtlsSocketContext();
    mSocket=(new DtlsFactory())->createClient(std::auto_ptr<DtlsSocketContext>(dtlsRtcp));
    dtlsRtcp->setSocket(mSocket);
    dtlsRtcp->setDtlsReceiver(this);
  }
  bundle_ = bundle;
  nice_ = new NiceConnection(med, transport_name, comps, stunServer, stunPort, minPort, maxPort);
  nice_->setNiceListener(this);
  nice_->start();
}

DtlsTransport::~DtlsTransport() {
  ELOG_DEBUG("DTLSTransport destructor");
  delete nice_;
  nice_ = NULL;
  if (dtlsRtp != NULL) {
    dtlsRtp->stop();
    //      delete dtlsRtp;
  }
  if (dtlsRtcp != NULL) {
    dtlsRtcp->stop();
    //      delete dtlsRtcp;
  }
  delete srtp_;
  srtp_=NULL;
  delete srtcp_;
  srtcp_=NULL;
}

void DtlsTransport::onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice) {
  boost::mutex::scoped_lock lock(readMutex_);
  int length = len;
  SrtpChannel *srtp = srtp_;

  if (DtlsTransport::isDtlsPacket(data, len)) {
    ELOG_DEBUG("Received DTLS message from %u", component_id);
    if (component_id == 1) {
      dtlsRtp->read(reinterpret_cast<unsigned char*>(data), len);
    } else {
      dtlsRtcp->read(reinterpret_cast<unsigned char*>(data), len);
    }

    return;
  } else if (this->getTransportState() == TRANSPORT_READY) {
    memset(unprotectBuf_, 0, len);
    memcpy(unprotectBuf_, data, len);

    if (dtlsRtcp != NULL && component_id == 2) {
      srtp = srtcp_;
    }

    if (srtp != NULL){
      rtcpheader *chead = reinterpret_cast<rtcpheader*> (unprotectBuf_);
      if (chead->packettype == RTCP_Sender_PT || 
          chead->packettype == RTCP_Receiver_PT || 
          chead->packettype == RTCP_PS_Feedback_PT||
          chead->packettype == RTCP_RTP_Feedback_PT){
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

void DtlsTransport::write(char* data, int len) {
  boost::mutex::scoped_lock lock(writeMutex_);
  if (nice_==NULL)
    return;
  int length = len;
  SrtpChannel *srtp = srtp_;

  int comp = 1;
  if (this->getTransportState() == TRANSPORT_READY) {
    memset(protectBuf_, 0, len);
    memcpy(protectBuf_, data, len);

    rtcpheader *chead = reinterpret_cast<rtcpheader*> (protectBuf_);
    if (chead->packettype == RTCP_Sender_PT || chead->packettype == RTCP_Receiver_PT || chead->packettype == RTCP_PS_Feedback_PT||chead->packettype == RTCP_RTP_Feedback_PT) {
      if (!rtcp_mux_) {
        comp = 2;
      }
      if (dtlsRtcp != NULL) {
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

void DtlsTransport::writeDtls(DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) {
  int comp = 1;
  if (ctx == dtlsRtcp) {
    comp = 2;
  }
  ELOG_DEBUG("Sending DTLS message to %d", comp);
  nice_->sendData(comp, data, len);
}

void DtlsTransport::onHandshakeCompleted(DtlsSocketContext *ctx, std::string clientKey,std::string serverKey, std::string srtp_profile) {
  if (ctx == dtlsRtp) {
    ELOG_DEBUG("Setting RTP srtp params");
    srtp_ = new SrtpChannel();
    if (srtp_->setRtpParams((char*) clientKey.c_str(), (char*) serverKey.c_str())) {
      readyRtp = true;
    } else {
      updateTransportState(TRANSPORT_FAILED);
    }
    if (dtlsRtcp == NULL) {
      readyRtcp = true;
    }
  }
  if (ctx == dtlsRtcp) {
    ELOG_DEBUG("Setting RTCP srtp params");
    srtcp_ = new SrtpChannel();
    if (srtcp_->setRtpParams((char*) clientKey.c_str(), (char*) serverKey.c_str())) {
      readyRtcp = true;
    } else {
      updateTransportState(TRANSPORT_FAILED);
    }
  }
  if (readyRtp && readyRtcp) {
    updateTransportState(TRANSPORT_READY);
  }

}

std::string DtlsTransport::getMyFingerprint() {
  return dtlsRtp->getFingerprint();
}

void DtlsTransport::updateIceState(IceState state, NiceConnection *conn) {
  ELOG_DEBUG( "New NICE state %d %d %d", state, mediaType, bundle_ );
  if (state == NICE_CANDIDATES_GATHERED) {
    updateTransportState(TRANSPORT_STARTED);
  }
  if (state == NICE_READY) {
    dtlsRtp->start();
    if (dtlsRtcp != NULL) {
      dtlsRtcp->start();
    }
  }
}

void DtlsTransport::processLocalSdp(SdpInfo *localSdp_) {
  ELOG_DEBUG( "Processing Local SDP in DTLS Transport" );
  std::vector<CandidateInfo> *cands;
  localSdp_->isFingerprint = true;
  localSdp_->fingerprint = getMyFingerprint();
  if (nice_->iceState >= NICE_CANDIDATES_GATHERED) {
    cands = nice_->localCandidates;
    ELOG_DEBUG( "Candidates: %d", cands->size() );
    for (unsigned int it = 0; it < cands->size(); it++) {
      CandidateInfo cand = cands->at(it);
      cand.isBundle = bundle_;
      // TODO Check if bundle
      localSdp_->addCandidate(cand);
      if (cand.isBundle) {
        ELOG_DEBUG("Adding bundle candidate! %d", cand.mediaType);
        cand.mediaType = AUDIO_TYPE;
        localSdp_->addCandidate(cand);
      }
    }
  }
  ELOG_DEBUG( "Processed Local SDP in DTLS Transport" );
}

bool DtlsTransport::isDtlsPacket(const char* buf, int len) {
  int data = DtlsFactory::demuxPacket(reinterpret_cast<const unsigned char*>(buf),len);
  switch(data)
  {
    case DtlsFactory::dtls:
      return true;
      break;
    default:
      return false;
      break;
  }
}
