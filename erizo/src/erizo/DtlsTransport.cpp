/*
 * DtlsConnection.cpp
 */

#include "DtlsTransport.h"

#include <string>
#include <cstring>

#include "./SrtpChannel.h"
#include "rtp/RtpHeaders.h"
#include "lib/LibNiceInterface.h"

using erizo::Resender;
using erizo::DtlsTransport;
using dtls::DtlsSocketContext;

DEFINE_LOGGER(DtlsTransport, "DtlsTransport");
DEFINE_LOGGER(Resender, "Resender");

using std::memcpy;

Resender::Resender(DtlsTransport* transport, dtls::DtlsSocketContext* ctx, unsigned int resend_seconds,
  unsigned int max_resends): transport_(transport), socket_context_(ctx),
  resend_seconds_(resend_seconds), max_resends_(max_resends), timer(service) {
}

Resender::~Resender() {
  ELOG_DEBUG("message: ~Resender");
  timer.cancel();
  if (thread_.get() != NULL) {
    ELOG_DEBUG("message: ~Resender joining thread");
    thread_->join();
    ELOG_DEBUG("message: ~Resender thread terminated ");
  }
}

void Resender::Cancel() {
  ELOG_DEBUG("message: cancelled");
  timer.cancel();
}

void Resender::ScheduleResend(const unsigned char* data, unsigned int len) {
  ELOG_DEBUG("message: Scheduling a new resender");
  timer.cancel();
  memcpy(data_, data, len);
  len_ = len;
  transport_->writeDtlsPacket(socket_context_, data_, len_);
  if (thread_.get() != NULL) {
    ELOG_DEBUG("message: join thread before scheduling resend");
    thread_->join();
    ELOG_DEBUG("message: thread joined successfully");
  }

  timer.expires_from_now(boost::posix_time::seconds(resend_seconds_));
  timer.async_wait(boost::bind(&Resender::Resend, this, boost::asio::placeholders::error));
  thread_.reset(new boost::thread(boost::bind(&Resender::Run, this)));
}


void Resender::Run() {
  service.run();
}

void Resender::Resend(const boost::system::error_code& ec) {
  if (ec == boost::asio::error::operation_aborted) {
    ELOG_DEBUG("message: skipping cancelled resend");
    return;
  }

  if (transport_ != NULL) {
    if (max_resends_-- > 0) {
      ELOG_DEBUG("message: Resending DTLS message");
      transport_->writeDtlsPacket(socket_context_, data_, len_);
      timer.expires_from_now(boost::posix_time::seconds(resend_seconds_));
      timer.async_wait(boost::bind(&Resender::Resend, this, boost::asio::placeholders::error));
    } else {
      ELOG_DEBUG("message: DTLS Timeout");
      transport_->onHandshakeFailed(socket_context_, "Dtls Timeout on Resender");
    }
  }
}

DtlsTransport::DtlsTransport(MediaType med, const std::string &transport_name, const std::string& connection_id,
                            bool bundle, bool rtcp_mux, TransportListener *transportListener,
                            const IceConfig& iceConfig, std::string username, std::string password, bool isServer):
  Transport(med, transport_name, connection_id, bundle, rtcp_mux, transportListener, iceConfig),
  readyRtp(false), readyRtcp(false), running_(false), isServer_(isServer) {
    ELOG_DEBUG("%s, message: constructor, transportName:%s, isBundle:%d", toLog(), transport_name.c_str(), bundle);
    dtlsRtp.reset(new DtlsSocketContext());

    int comps = 1;
    if (isServer_) {
      ELOG_DEBUG("%s, message: creating  passive-server", toLog());
      dtlsRtp->createServer();
      dtlsRtp->setDtlsReceiver(this);

      if (!rtcp_mux) {
        comps = 2;
        dtlsRtcp.reset(new DtlsSocketContext());
        dtlsRtcp->createServer();
        dtlsRtcp->setDtlsReceiver(this);
      }
    } else {
      ELOG_DEBUG("%s, message: creating active-client", toLog());
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
    rtp_resender_.reset(new Resender(this, dtlsRtp.get(), kSecsPerResend, kMaxResends));
    if (!rtcp_mux) {
      rtcp_resender_.reset(new Resender(this, dtlsRtcp.get(), kSecsPerResend, kMaxResends));
    }
    running_ = true;
    getNice_Thread_ = boost::thread(&DtlsTransport::getNiceDataLoop, this);
    ELOG_DEBUG("%s, message: created", toLog());
  }

DtlsTransport::~DtlsTransport() {
  ELOG_DEBUG("%s, message: destroying", toLog());
  if (this->state_ != TRANSPORT_FINISHED) {
    this->close();
  }
  ELOG_DEBUG("%s, message: destroyed", toLog());
}

void DtlsTransport::start() {
  ELOG_DEBUG("%s, message: starting ice", toLog());
  nice_->start();
}

void DtlsTransport::close() {
  ELOG_DEBUG("%s, message: closing", toLog());
  running_ = false;
  nice_->setNiceListener(NULL);
  nice_->close();
  this->state_ = TRANSPORT_FINISHED;
  getNice_Thread_.join();
  ELOG_DEBUG("%s, message: closed", toLog());
}

void DtlsTransport::onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice) {
  int length = len;
  SrtpChannel *srtp = srtp_.get();
  if (DtlsTransport::isDtlsPacket(data, len)) {
    ELOG_DEBUG("%s, message: Received DTLS message, transportName: %s, componentId: %u",
               toLog(), transport_name.c_str(), component_id);
    if (component_id == 1) {
      if (rtp_resender_.get() != NULL) {
        rtp_resender_->Cancel();
      }
      dtlsRtp->read(reinterpret_cast<unsigned char*>(data), len);
    } else {
      if (rtcp_resender_.get() != NULL) {
        rtcp_resender_->Cancel();
      }
      dtlsRtcp->read(reinterpret_cast<unsigned char*>(data), len);
    }
    return;
  } else if (this->getTransportState() == TRANSPORT_READY) {
    memcpy(unprotectBuf_, data, len);

    if (dtlsRtcp != NULL && component_id == 2) {
      srtp = srtcp_.get();
    }
    if (srtp != NULL) {
      RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(unprotectBuf_);
      if (chead->isRtcp()) {
        if (srtp->unprotectRtcp(unprotectBuf_, &length) < 0) {
          return;
        }
      } else {
        if (srtp->unprotectRtp(unprotectBuf_, &length) < 0) {
          return;
        }
      }
    } else {
      return;
    }

    if (length <= 0) {
      return;
    }
    getTransportListener()->onTransportData(unprotectBuf_, length, this);
  }
}

void DtlsTransport::onCandidate(const CandidateInfo &candidate, NiceConnection *conn) {
  getTransportListener()->onCandidate(candidate, this);
}

void DtlsTransport::write(char* data, int len) {
  boost::mutex::scoped_lock lock(writeMutex_);
  if (nice_ == NULL)
    return;
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
      this->writeOnNice(comp, protectBuf_, length);
    }
  }
}

void DtlsTransport::onDtlsPacket(DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) {
  int comp = 1;
  if (ctx == dtlsRtcp.get()) {
    comp = 2;
    rtcp_resender_->ScheduleResend(data, len);
  } else {
    rtp_resender_->ScheduleResend(data, len);
  }

  ELOG_DEBUG("%s, message: Sending DTLS message, transportName: %s, componentId: %d",
             toLog(), transport_name.c_str(), comp);
}

void DtlsTransport::writeDtlsPacket(DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) {
    int comp = 1;
    if (ctx == dtlsRtcp.get()) {
      comp = 2;
    }
    nice_->sendData(comp, data, len);
}

void DtlsTransport::onHandshakeCompleted(DtlsSocketContext *ctx, std::string clientKey, std::string serverKey,
                                         std::string srtp_profile) {
  boost::mutex::scoped_lock lock(sessionMutex_);
  std::string temp;

  if (isServer_) {  // If we are server, we swap the keys
    ELOG_DEBUG("%s, message: swapping keys, isServer: %d", toLog(), isServer_);
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
  ELOG_DEBUG("%s, message:HandShakeCompleted, transportName:%s, readyRtp:%d, readyRtcp:%d",
             toLog(), transport_name.c_str(), readyRtp, readyRtcp);
  if (readyRtp && readyRtcp) {
    updateTransportState(TRANSPORT_READY);
  }
}

void DtlsTransport::onHandshakeFailed(DtlsSocketContext *ctx, const std::string error) {
  ELOG_WARN("%s, message: Handshake failed, transportName:%s, openSSLerror: %s",
            toLog(), transport_name.c_str(), error.c_str());
  running_ = false;
  updateTransportState(TRANSPORT_FAILED);
}

std::string DtlsTransport::getMyFingerprint() {
  return dtlsRtp->getFingerprint();
}

void DtlsTransport::updateIceState(IceState state, NiceConnection *conn) {
  ELOG_DEBUG("%s, message:NiceState, transportName: %s, state: %d, isBundle: %d",
             toLog(), transport_name.c_str(), state, bundle_);
  if (state == NICE_INITIAL && this->getTransportState() != TRANSPORT_STARTED) {
    updateTransportState(TRANSPORT_STARTED);
  } else if (state == NICE_CANDIDATES_RECEIVED && this->getTransportState() != TRANSPORT_GATHERED) {
    updateTransportState(TRANSPORT_GATHERED);
  } else if (state == NICE_FAILED) {
    ELOG_DEBUG("%s, message: Nice Failed", toLog());
    running_ = false;
    updateTransportState(TRANSPORT_FAILED);
  } else if (state == NICE_READY) {
    if (!isServer_ && dtlsRtp && !dtlsRtp->started) {
      ELOG_INFO("%s, message: DTLSRTP Start, transportName: %s", toLog(), transport_name.c_str());
      dtlsRtp->start();
    }
    if (!isServer_ && dtlsRtcp != NULL && !dtlsRtcp->started) {
      ELOG_DEBUG("%s, message: DTLSRTCP Start, transportName: %s", toLog(), transport_name.c_str());
      dtlsRtcp->start();
    }
  }
}

void DtlsTransport::processLocalSdp(SdpInfo *localSdp_) {
  ELOG_DEBUG("%s, message: processing local sdp, transportName: %s", toLog(), transport_name.c_str());
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
  ELOG_DEBUG("%s, message: processed local sdp, transportName: %s, ufrag: %s, pass: %s",
             toLog(), transport_name.c_str(), username.c_str(), password.c_str());
}

void DtlsTransport::getNiceDataLoop() {
  while (running_) {
    p_ = nice_->getPacket();
    if (p_->length > 0) {
      this->onNiceData(p_->comp, p_->data, p_->length, NULL);
    }
    if (p_->length == -1) {
      ELOG_DEBUG("%s, message: closing packet loop", toLog());
      running_ = false;
      return;
    }
  }
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
