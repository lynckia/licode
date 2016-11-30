/*
 * DtlsConnection.cpp
 */

#include "DtlsTransport.h"

#include <string>
#include <cstring>
#include <memory>

#include "./SrtpChannel.h"
#include "rtp/RtpHeaders.h"
#include "lib/LibNiceInterface.h"

using erizo::Resender;
using erizo::DtlsTransport;
using erizo::packetPtr;
using dtls::DtlsSocketContext;

DEFINE_LOGGER(DtlsTransport, "DtlsTransport");
DEFINE_LOGGER(Resender, "Resender");

using std::memcpy;

Resender::Resender(DtlsTransport* transport, dtls::DtlsSocketContext* ctx)
    : transport_(transport), socket_context_(ctx),
      resend_seconds_(kInitialSecsPerResend), max_resends_(kMaxResends) {
}

Resender::~Resender() {
  ELOG_DEBUG("message: ~Resender");
  cancel();
}

void Resender::cancel() {
  ELOG_DEBUG("message: cancelled");
  transport_->getWorker()->unschedule(scheduled_task_);
}

void Resender::scheduleResend(packetPtr packet) {
  ELOG_DEBUG("message: Scheduling a new resender");
  transport_->getWorker()->unschedule(scheduled_task_);
  resend_seconds_ = kInitialSecsPerResend;
  packet_ = packet;
  transport_->writeDtlsPacket(socket_context_, packet_);
  scheduleNext();
}

void Resender::resend() {
  if (transport_ != nullptr) {
    if (max_resends_-- > 0) {
      ELOG_DEBUG("%s message: Resending DTLS message", transport_->toLog());
      transport_->writeDtlsPacket(socket_context_, packet_);
      scheduleNext();
    } else {
      ELOG_DEBUG("%s message: DTLS timeout", transport_->toLog());
      transport_->onHandshakeFailed(socket_context_, "Dtls Timeout on Resender");
    }
  }
}

void Resender::scheduleNext() {
  scheduled_task_ = transport_->getWorker()->scheduleFromNow([this]() {
    this->resend();
  }, std::chrono::seconds(resend_seconds_));
  resend_seconds_ = resend_seconds_ * 2;
}

DtlsTransport::DtlsTransport(MediaType med, const std::string &transport_name, const std::string& connection_id,
                            bool bundle, bool rtcp_mux, TransportListener *transportListener,
                            const IceConfig& iceConfig, std::string username, std::string password,
                            bool isServer, std::shared_ptr<Worker> worker):
  Transport(med, transport_name, connection_id, bundle, rtcp_mux, transportListener, iceConfig, worker),
  unprotect_packet_{std::make_shared<dataPacket>()},
  readyRtp(false), readyRtcp(false), isServer_(isServer) {
    ELOG_DEBUG("%s message: constructor, transportName:%s, isBundle:%d", toLog(), transport_name.c_str(), bundle);
    dtlsRtp.reset(new DtlsSocketContext());

    int comps = 1;
    if (isServer_) {
      ELOG_DEBUG("%s message: creating  passive-server", toLog());
      dtlsRtp->createServer();
      dtlsRtp->setDtlsReceiver(this);

      if (!rtcp_mux) {
        comps = 2;
        dtlsRtcp.reset(new DtlsSocketContext());
        dtlsRtcp->createServer();
        dtlsRtcp->setDtlsReceiver(this);
      }
    } else {
      ELOG_DEBUG("%s message: creating active-client", toLog());
      dtlsRtp->createClient();
      dtlsRtp->setDtlsReceiver(this);

      if (!rtcp_mux) {
        comps = 2;
        dtlsRtcp.reset(new DtlsSocketContext());
        dtlsRtcp->createClient();
        dtlsRtcp->setDtlsReceiver(this);
      }
    }
    nice_.reset(new NiceConnection(boost::shared_ptr<LibNiceInterface>(new LibNiceInterfaceImpl()), med,
          transport_name, connection_id_, this, comps, iceConfig_, username, password));
    rtp_resender_.reset(new Resender(this, dtlsRtp.get()));
    if (!rtcp_mux) {
      rtcp_resender_.reset(new Resender(this, dtlsRtcp.get()));
    }
    ELOG_DEBUG("%s message: created", toLog());
  }

DtlsTransport::~DtlsTransport() {
  ELOG_DEBUG("%s message: destroying", toLog());
  if (this->state_ != TRANSPORT_FINISHED) {
    this->close();
  }
  ELOG_DEBUG("%s message: destroyed", toLog());
}

void DtlsTransport::start() {
  nice_->copyLogContextFrom(this);
  ELOG_DEBUG("%s message: starting ice", toLog());
  nice_->start();
}

void DtlsTransport::close() {
  ELOG_DEBUG("%s message: closing", toLog());
  running_ = false;
  nice_->close();
  this->state_ = TRANSPORT_FINISHED;
  ELOG_DEBUG("%s message: closed", toLog());
}

void DtlsTransport::onNiceData(packetPtr packet) {
  int len = packet->length;
  char *data = packet->data;
  unsigned int component_id = packet->comp;

  int length = len;
  SrtpChannel *srtp = srtp_.get();
  if (DtlsTransport::isDtlsPacket(data, len)) {
    ELOG_DEBUG("%s message: Received DTLS message, transportName: %s, componentId: %u",
               toLog(), transport_name.c_str(), component_id);
    if (component_id == 1) {
      if (rtp_resender_.get() != NULL) {
        rtp_resender_->cancel();
      }
      dtlsRtp->read(reinterpret_cast<unsigned char*>(data), len);
    } else {
      if (rtcp_resender_.get() != NULL) {
        rtcp_resender_->cancel();
      }
      dtlsRtcp->read(reinterpret_cast<unsigned char*>(data), len);
    }
    return;
  } else if (this->getTransportState() == TRANSPORT_READY) {
    unprotect_packet_->length = len;
    memcpy(unprotect_packet_->data, data, len);

    if (dtlsRtcp != NULL && component_id == 2) {
      srtp = srtcp_.get();
    }
    if (srtp != NULL) {
      RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(unprotect_packet_->data);
      if (chead->isRtcp()) {
        if (srtp->unprotectRtcp(unprotect_packet_->data, &unprotect_packet_->length) < 0) {
          return;
        }
      } else {
        if (srtp->unprotectRtp(unprotect_packet_->data, &unprotect_packet_->length) < 0) {
          return;
        }
      }
    } else {
      return;
    }

    if (length <= 0) {
      return;
    }
    getTransportListener()->onTransportData(unprotect_packet_, this);
  }
}

void DtlsTransport::onCandidate(const CandidateInfo &candidate, NiceConnection *conn) {
  getTransportListener()->onCandidate(candidate, this);
}

void DtlsTransport::write(char* data, int len) {
  if (nice_ == nullptr || !running_) {
    return;
  }
  int length = len;
  SrtpChannel *srtp = srtp_.get();

  if (this->getTransportState() == TRANSPORT_READY) {
    memcpy(protectBuf_, data, len);
    int comp = 1;
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(protectBuf_);
    if (chead->isRtcp()) {
      if (!rtcp_mux_) {
        comp = 2;
      }
      if (dtlsRtcp != NULL) {
        srtp = srtcp_.get();
      }
      if (srtp && nice_->checkIceState() == NICE_READY) {
        if (srtp->protectRtcp(protectBuf_, &length) < 0) {
          return;
        }
      }
    } else {
      comp = 1;

      if (srtp && nice_->checkIceState() == NICE_READY) {
        if (srtp->protectRtp(protectBuf_, &length) < 0) {
          return;
        }
      }
    }
    if (length <= 10) {
      return;
    }
    if (nice_->checkIceState() == NICE_READY) {
      writeOnNice(comp, protectBuf_, length);
    }
  }
}

void DtlsTransport::onDtlsPacket(DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) {
  bool is_rtcp = ctx == dtlsRtcp.get();
  int component_id = is_rtcp ? 2 : 1;

  packetPtr packet = std::make_shared<dataPacket>(component_id, data, len);

  if (is_rtcp) {
    rtcp_resender_->scheduleResend(packet);
  } else {
    rtp_resender_->scheduleResend(packet);
  }

  ELOG_DEBUG("%s message: Sending DTLS message, transportName: %s, componentId: %d",
             toLog(), transport_name.c_str(), packet->comp);
}

void DtlsTransport::writeDtlsPacket(DtlsSocketContext *ctx, packetPtr packet) {
  char data[1500];
  unsigned int len = packet->length;
  memcpy(data, packet->data, len);
  writeOnNice(packet->comp, data, len);
}

void DtlsTransport::onHandshakeCompleted(DtlsSocketContext *ctx, std::string clientKey, std::string serverKey,
                                         std::string srtp_profile) {
  boost::mutex::scoped_lock lock(sessionMutex_);
  std::string temp;

  if (isServer_) {  // If we are server, we swap the keys
    ELOG_DEBUG("%s message: swapping keys, isServer: %d", toLog(), isServer_);
    clientKey.swap(serverKey);
  }
  if (ctx == dtlsRtp.get()) {
    srtp_.reset(new SrtpChannel());
    if (srtp_->setRtpParams(clientKey, serverKey)) {
      readyRtp = true;
    } else {
      updateTransportState(TRANSPORT_FAILED);
    }
    if (dtlsRtcp == NULL) {
      readyRtcp = true;
    }
  }
  if (ctx == dtlsRtcp.get()) {
    srtcp_.reset(new SrtpChannel());
    if (srtcp_->setRtpParams(clientKey, serverKey)) {
      readyRtcp = true;
    } else {
      updateTransportState(TRANSPORT_FAILED);
    }
  }
  ELOG_DEBUG("%s message:HandShakeCompleted, transportName:%s, readyRtp:%d, readyRtcp:%d",
             toLog(), transport_name.c_str(), readyRtp, readyRtcp);
  if (readyRtp && readyRtcp) {
    updateTransportState(TRANSPORT_READY);
  }
}

void DtlsTransport::onHandshakeFailed(DtlsSocketContext *ctx, const std::string error) {
  ELOG_WARN("%s message: Handshake failed, transportName:%s, openSSLerror: %s",
            toLog(), transport_name.c_str(), error.c_str());
  running_ = false;
  updateTransportState(TRANSPORT_FAILED);
}

std::string DtlsTransport::getMyFingerprint() {
  return dtlsRtp->getFingerprint();
}

void DtlsTransport::updateIceState(IceState state, NiceConnection *conn) {
  ELOG_DEBUG("%s message:NiceState, transportName: %s, state: %d, isBundle: %d",
             toLog(), transport_name.c_str(), state, bundle_);
  if (state == NICE_INITIAL && this->getTransportState() != TRANSPORT_STARTED) {
    updateTransportState(TRANSPORT_STARTED);
  } else if (state == NICE_CANDIDATES_RECEIVED && this->getTransportState() != TRANSPORT_GATHERED) {
    updateTransportState(TRANSPORT_GATHERED);
  } else if (state == NICE_FAILED) {
    ELOG_DEBUG("%s message: Nice Failed", toLog());
    running_ = false;
    updateTransportState(TRANSPORT_FAILED);
  } else if (state == NICE_READY) {
    if (!isServer_ && dtlsRtp && !dtlsRtp->started) {
      ELOG_INFO("%s message: DTLSRTP Start, transportName: %s", toLog(), transport_name.c_str());
      dtlsRtp->start();
    }
    if (!isServer_ && dtlsRtcp != NULL && !dtlsRtcp->started) {
      ELOG_DEBUG("%s message: DTLSRTCP Start, transportName: %s", toLog(), transport_name.c_str());
      dtlsRtcp->start();
    }
  }
}

void DtlsTransport::processLocalSdp(SdpInfo *localSdp_) {
  ELOG_DEBUG("%s message: processing local sdp, transportName: %s", toLog(), transport_name.c_str());
  localSdp_->isFingerprint = true;
  localSdp_->fingerprint = getMyFingerprint();
  std::string username = nice_->getLocalUsername();
  std::string password = nice_->getLocalPassword();
  if (bundle_) {
    localSdp_->setCredentials(username, password, VIDEO_TYPE);
    localSdp_->setCredentials(username, password, AUDIO_TYPE);
  } else {
    localSdp_->setCredentials(username, password, this->mediaType);
  }
  ELOG_DEBUG("%s message: processed local sdp, transportName: %s, ufrag: %s, pass: %s",
             toLog(), transport_name.c_str(), username.c_str(), password.c_str());
}

bool DtlsTransport::isDtlsPacket(const char* buf, int len) {
  int data = DtlsSocketContext::demuxPacket(reinterpret_cast<const unsigned char*>(buf), len);
  switch (data) {
    case DtlsSocketContext::dtls:
      return true;
      break;
    default:
      return false;
      break;
  }
}
