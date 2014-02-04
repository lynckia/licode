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
DEFINE_LOGGER(Resender, "Resender");

Resender::Resender(NiceConnection *nice, unsigned int comp, const unsigned char* data, unsigned int len) : 
  nice_(nice), comp_(comp), data_(data),len_(len), timer(service) {
}

Resender::~Resender() {
  timer.cancel();
  thread_->join();
}

void Resender::cancel() {
  timer.cancel();
}

void Resender::start() {
  timer.cancel();
  if (thread_.get()!=NULL) {
    thread_->join();
  }
  timer.expires_from_now(boost::posix_time::seconds(3));
  timer.async_wait(boost::bind(&Resender::resend, this, boost::asio::placeholders::error));
  thread_.reset(new boost::thread(boost::bind(&Resender::run, this)));
}

void Resender::run() {
  service.run();
}

void Resender::resend(const boost::system::error_code& ec) {  
  if (ec == boost::asio::error::operation_aborted) {
    if (nice_ != NULL) {
      ELOG_DEBUG("%s - Cancelled", nice_->transportName->c_str());
    }
    return;
  }
  
  if (nice_ != NULL) {
    ELOG_WARN("%s - Resending DTLS message to %d", nice_->transportName->c_str(), comp_);
    nice_->sendData(comp_, data_, len_);
  }
}

DtlsTransport::DtlsTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, TransportListener *transportListener, const std::string &stunServer, int stunPort, int minPort, int maxPort):Transport(med, transport_name, bundle, rtcp_mux, transportListener, stunServer, stunPort, minPort, maxPort) {
  ELOG_DEBUG( "Initializing DtlsTransport" );
  updateTransportState(TRANSPORT_INITIAL);

  readyRtp = false;
  readyRtcp = false;

  dtlsRtp.reset(new DtlsSocketContext());

  DtlsSocket *mSocket=(new DtlsFactory())->createClient(dtlsRtp);
  dtlsRtp->setSocket(mSocket);
  dtlsRtp->setDtlsReceiver(this);

  srtp_ = NULL;
  srtcp_ = NULL;
  int comps = 1;
  if (!rtcp_mux) {
    comps = 2;
    dtlsRtcp.reset(new DtlsSocketContext());
    mSocket=(new DtlsFactory())->createClient(dtlsRtcp);
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
    ELOG_DEBUG("%s - Received DTLS message from %u", transport_name.c_str(), component_id);
    if (component_id == 1) {
      rtpResender->cancel();
      dtlsRtp->read(reinterpret_cast<unsigned char*>(data), len);
    } else {
      rtcpResender->cancel();
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
      this->writeOnNice(comp, protectBuf_, length);
    }
  }
}

void DtlsTransport::writeDtls(DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) {
  int comp = 1;
  if (ctx == dtlsRtcp.get()) {
    comp = 2;
    rtcpResender.reset(new Resender(nice_, comp, data, len));
    rtcpResender->start();
  } else {
    rtpResender.reset(new Resender(nice_, comp, data, len));
    rtpResender->start();
  }
      
  ELOG_DEBUG("%s - Sending DTLS message to %d", transport_name.c_str(), comp);

  nice_->sendData(comp, data, len);
}

void DtlsTransport::onHandshakeCompleted(DtlsSocketContext *ctx, std::string clientKey,std::string serverKey, std::string srtp_profile) {
  boost::mutex::scoped_lock lock(sessionMutex_);
  if (ctx == dtlsRtp.get()) {
    ELOG_DEBUG("%s - Setting RTP srtp params", transport_name.c_str());
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
  if (ctx == dtlsRtcp.get()) {
    ELOG_DEBUG("%s - Setting RTCP srtp params", transport_name.c_str());
    srtcp_ = new SrtpChannel();
    if (srtcp_->setRtpParams((char*) clientKey.c_str(), (char*) serverKey.c_str())) {
      readyRtcp = true;
    } else {
      updateTransportState(TRANSPORT_FAILED);
    }
  }
  ELOG_DEBUG("%s - Ready? %d %d", transport_name.c_str(), readyRtp, readyRtcp);
  if (readyRtp && readyRtcp) {
    ELOG_DEBUG("%s - Ready!!!", transport_name.c_str());
    updateTransportState(TRANSPORT_READY);
  }

}

std::string DtlsTransport::getMyFingerprint() {
  return dtlsRtp->getFingerprint();
}

void DtlsTransport::updateIceState(IceState state, NiceConnection *conn) {
  ELOG_DEBUG( "%s - New NICE state %d %d %d", transport_name.c_str(), state, mediaType, bundle_);
  if (state == NICE_CANDIDATES_GATHERED && this->getTransportState() != TRANSPORT_STARTED) {
    updateTransportState(TRANSPORT_STARTED);
  }
  if (state == NICE_READY) {
    ELOG_DEBUG("%s - Nice ready", transport_name.c_str());
    if (!dtlsRtp->started) {
      ELOG_DEBUG("%s - DTLSRTP Start", transport_name.c_str());
      dtlsRtp->start();
    }
    if (dtlsRtcp != NULL && !dtlsRtcp->started) {
      ELOG_DEBUG("%s - DTLSRTCP Start", transport_name.c_str());
      dtlsRtcp->start();
    }
  }
}

void DtlsTransport::processLocalSdp(SdpInfo *localSdp_) {
  ELOG_DEBUG( "Processing Local SDP in DTLS Transport" );
  localSdp_->isFingerprint = true;
  localSdp_->fingerprint = getMyFingerprint();
  if (nice_->iceState >= NICE_CANDIDATES_GATHERED) {

    boost::shared_ptr<std::vector<CandidateInfo> > cands = nice_->localCandidates;
    ELOG_DEBUG( "Candidates: %lu", cands->size() );
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
