/*
 * DtlsConnection.cpp
 */
#include <iostream>
#include <cassert>

#include "DtlsTransport.h"
#include "NiceConnection.h"
#include "SrtpChannel.h"

#include "dtls/DtlsFactory.h"

using namespace erizo;
using namespace std;
using namespace dtls;

DtlsTransport::DtlsTransport(MediaType med, const std::string &transport_name, bool rtcp_mux, TransportListener *transportListener):Transport(mediaType, transport_name, rtcp_mux, transportListener) {
    cout << "Initializing DtlsTransport" << endl;
    updateTransportState(TRANSPORT_INITIAL);
    dtlsClient = new DtlsSocketContext();

    DtlsSocket *mSocket=DtlsFactory::GetInstance()->createClient(std::auto_ptr<DtlsSocketContext>(dtlsClient));
    dtlsClient->setSocket(mSocket);
    dtlsClient->setDtlsReceiver(this);
    srtp_ = NULL;
    protectBuf_ =reinterpret_cast<char*>(malloc(10000));
    unprotectBuf_ =reinterpret_cast<char*>(malloc(10000));
    nice_ = new NiceConnection(med, transport_name, 1);
    nice_->setNiceListener(this);
    nice_->start();
}

DtlsTransport::~DtlsTransport() {

  this->close();

  free(protectBuf_);
  free(unprotectBuf_);

}

void DtlsTransport::close() {
  if (dtlsClient != NULL) {
      dtlsClient->stop();
  }
  if (nice_ != NULL) {
     nice_->close();
     nice_->join();
     delete nice_;
  }
}

void DtlsTransport::onNiceData(char* data, int len, NiceConnection* nice) {
    boost::mutex::scoped_lock lock(readMutex_);
    int length = len;
    bool isFeedback = false;

    int recvSSRC = 0;
    if (DtlsTransport::isDtlsPacket(data, len)) {
        dtlsClient->read(reinterpret_cast<unsigned char*>(data), len);
        return;
    }
    memset(unprotectBuf_, 0, len);
    memcpy(unprotectBuf_, data, len);

    if (srtp_ != NULL){
        rtcpheader *chead = reinterpret_cast<rtcpheader*> (unprotectBuf_);
        if (chead->packettype == 200) { //Sender Report
          if (srtp_->unprotectRtcp(unprotectBuf_, &length)<0)
            return;
          recvSSRC = ntohl(chead->ssrc);
        } else if (chead->packettype == 201 || chead->packettype == 206){
          if(srtp_->unprotectRtcp(unprotectBuf_, &length)<0)
            return;
          recvSSRC = ntohl(chead->ssrcsource);
          isFeedback = true;
          // OnFeedback
        } else {
          if(srtp_->unprotectRtp(unprotectBuf_, &length)<0)
            return;
        }
    } else {
      return;
    }

    if (length <= 0)
        return;

    if (recvSSRC==0){
        rtpheader* inHead = reinterpret_cast<rtpheader*> (unprotectBuf_);
        recvSSRC= ntohl(inHead->ssrc);
    }

    getTransportListener()->onTransportData(unprotectBuf_, length, isFeedback, recvSSRC, this);
}

void DtlsTransport::write(char* data, int len, int sinkSSRC) {
   boost::mutex::scoped_lock lock(writeMutex_);
    int length = len;

    memset(protectBuf_, 0, len);
    memcpy(protectBuf_, data, len);

    rtcpheader *chead = reinterpret_cast<rtcpheader*> (protectBuf_);
    if (chead->packettype == 200 || chead->packettype == 201 || chead->packettype == 206) {
      chead->ssrc=htonl(sinkSSRC);
      if (srtp_ && nice_->iceState == NICE_READY) {
        if(srtp_->protectRtcp(protectBuf_, &length)<0) {
          return;
        }
      }
    }
    else{
      //rtpheader* inHead = reinterpret_cast<rtpheader*> (protectBuf_);
      //inHead->ssrc = htonl(sinkSSRC);
      if (srtp_ && nice_->iceState == NICE_READY) {
        if(srtp_->protectRtp(protectBuf_, &length)<0) {
          return;
        }
      }
    }
    if (length <= 10) {
      return;
    }
    if (nice_->iceState == NICE_READY) {
        getTransportListener()->queueData(protectBuf_, length);
    }
}

void DtlsTransport::writeDtls(const unsigned char* data, unsigned int len) {
    nice_->sendData(data, len);
}

void DtlsTransport::onHandshakeCompleted(std::string clientKey,std::string serverKey, std::string srtp_profile) {
    srtp_ = new SrtpChannel();

    srtp_->setRtpParams((char*) clientKey.c_str(), (char*) serverKey.c_str());
    srtp_->setRtcpParams((char*) clientKey.c_str(), (char*) serverKey.c_str());
    updateTransportState(TRANSPORT_READY);
}

std::string DtlsTransport::getMyFingerprint() {
    return dtlsClient->getFingerprint();
}

void DtlsTransport::updateIceState(IceState state, NiceConnection *conn) {
    cout << "New NICE state " << state << endl;
    if (state == NICE_CANDIDATES_GATHERED) {
      updateTransportState(TRANSPORT_STARTED);
    }
    if (state == NICE_READY) {
        dtlsClient->start();
    }
}

void DtlsTransport::processLocalSdp(SdpInfo *localSdp_) {
  cout << "Processing Local SDP in DTLS Transport" << endl;
  std::vector<CandidateInfo> *cands;
  localSdp_->isFingerprint = true;
  localSdp_->fingerprint = getMyFingerprint();
  if (nice_->iceState >= NICE_CANDIDATES_GATHERED) {
    cands = nice_->localCandidates;
    cout << " Candidates: " << cands->size() << endl;
    for (unsigned int it = 0; it < cands->size(); it++) {
      CandidateInfo cand = cands->at(it);
      cand.isBundle = true;
      // TODO Check if bundle
      localSdp_->addCandidate(cand);
      cand.mediaType = AUDIO_TYPE;
      localSdp_->addCandidate(cand);
    }
  }
  cout << "Processed Local SDP in DTLS Transport" << endl;
}

bool DtlsTransport::isDtlsPacket(const char* buf, int len) {
    int data = DtlsFactory::demuxPacket(reinterpret_cast<const unsigned char*>(buf),len);
    //cout << "Checking DTLS: " << data << endl;
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