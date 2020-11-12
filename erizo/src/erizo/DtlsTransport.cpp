/*
 * DtlsConnection.cpp
 */

#include "DtlsTransport.h"

#include <string>
#include <cstring>
#include <memory>

#include "./SrtpChannel.h"
#include "rtp/RtpHeaders.h"
#include "./NicerConnection.h"

using erizo::TimeoutChecker;
using erizo::DtlsTransport;
using dtls::DtlsSocketContext;

DEFINE_LOGGER(DtlsTransport, "DtlsTransport");
DEFINE_LOGGER(TimeoutChecker, "TimeoutChecker");

using std::memcpy;

static std::mutex dtls_mutex;

TimeoutChecker::TimeoutChecker(DtlsTransport* transport, dtls::DtlsSocketContext* ctx)
    : transport_(transport), socket_context_(ctx),
      check_seconds_(kInitialSecsPerTimeoutCheck), max_checks_(kMaxTimeoutChecks),
      scheduled_task_{std::make_shared<ScheduledTaskReference>()} {
}

TimeoutChecker::~TimeoutChecker() {
  cancel();
}

void TimeoutChecker::cancel() {
  transport_->getWorker()->unschedule(scheduled_task_);
}

void TimeoutChecker::scheduleCheck() {
  ELOG_TRACE("message: Scheduling a new TimeoutChecker");
  transport_->getWorker()->unschedule(scheduled_task_);
  check_seconds_ = kInitialSecsPerTimeoutCheck;
  if (transport_->getTransportState() != TRANSPORT_READY) {
    scheduleNext();
  }
}

void TimeoutChecker::scheduleNext() {
  scheduled_task_ = transport_->getWorker()->scheduleFromNow([this]() {
      if (transport_ != nullptr) {
        if (transport_->getTransportState() == TRANSPORT_READY) {
          return;
        }
        if (max_checks_-- > 0) {
          ELOG_DEBUG("Handling dtls timeout, checks left: %d", max_checks_);
          if (socket_context_) {
            std::lock_guard<std::mutex> guard(dtls_mutex);
            socket_context_->handleTimeout();
          }
          scheduleNext();
        } else {
          ELOG_DEBUG("%s message: DTLS timeout", transport_->toLog());
          transport_->onHandshakeFailed(socket_context_, "Dtls Timeout on TimeoutChecker");
        }
      }
  }, std::chrono::seconds(check_seconds_));
}

DtlsTransport::DtlsTransport(MediaType med, const std::string &transport_name, const std::string& connection_id,
                            bool bundle, bool rtcp_mux, std::weak_ptr<TransportListener> transport_listener,
                            const IceConfig& iceConfig, std::string username, std::string password,
                            bool isServer, std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker):
  Transport(med, transport_name, connection_id, bundle, rtcp_mux, transport_listener, iceConfig, worker, io_worker),
  readyRtp(false), readyRtcp(false), isServer_(isServer) {
    ELOG_DEBUG("%s message: constructor, transportName: %s, isBundle: %d", toLog(), transport_name.c_str(), bundle);
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
    iceConfig_.connection_id = connection_id_;
    iceConfig_.transport_name = transport_name;
    iceConfig_.media_type = med;
    iceConfig_.ice_components = comps;
    iceConfig_.username = username;
    iceConfig_.password = password;
    ice_ = NicerConnection::create(io_worker_, iceConfig_);

    rtp_timeout_checker_.reset(new TimeoutChecker(this, dtlsRtp.get()));
    if (!rtcp_mux) {
      rtcp_timeout_checker_.reset(new TimeoutChecker(this, dtlsRtcp.get()));
    }
    ELOG_DEBUG("%s message: created", toLog());
  }

DtlsTransport::~DtlsTransport() {
  if (this->state_ != TRANSPORT_FINISHED) {
    this->close();
  }
}

void DtlsTransport::start() {
  ice_->setIceListener(shared_from_this());
  ice_->copyLogContextFrom(*this);
  ELOG_DEBUG("%s message: starting ice", toLog());
  ice_->start();
}

void DtlsTransport::close() {
  ELOG_DEBUG("%s message: closing", toLog());
  running_ = false;
  if (rtp_timeout_checker_) {
    rtp_timeout_checker_->cancel();
  }
  if (rtcp_timeout_checker_) {
    rtcp_timeout_checker_->cancel();
  }
  ice_->close();
  if (dtlsRtp) {
    dtlsRtp->close();
  }
  if (dtlsRtcp) {
    dtlsRtcp->close();
  }
  this->state_ = TRANSPORT_FINISHED;
  ELOG_DEBUG("%s message: closed", toLog());
}

void DtlsTransport::onIceData(packetPtr packet) {
  if (!running_) {
    return;
  }
  int len = packet->length;
  char *data = packet->data;
  unsigned int component_id = packet->comp;

  int length = len;
  SrtpChannel *srtp = srtp_.get();
  if (DtlsTransport::isDtlsPacket(data, len)) {
    ELOG_DEBUG("%s message: Received DTLS message, transportName: %s, componentId: %u",
               toLog(), transport_name.c_str(), component_id);
    if (component_id == 1) {
      std::lock_guard<std::mutex> guard(dtls_mutex);
      dtlsRtp->read(reinterpret_cast<unsigned char*>(data), len);
    } else {
      std::lock_guard<std::mutex> guard(dtls_mutex);
      dtlsRtcp->read(reinterpret_cast<unsigned char*>(data), len);
    }
    return;
  } else if (this->getTransportState() == TRANSPORT_READY) {
    std::shared_ptr<DataPacket> unprotect_packet = std::make_shared<DataPacket>(component_id,
      data, len, VIDEO_PACKET, packet->received_time_ms);

    if (dtlsRtcp != NULL && component_id == 2) {
      srtp = srtcp_.get();
    }
    if (srtp != NULL) {
      RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(unprotect_packet->data);
      if (chead->isRtcp()) {
        if (srtp->unprotectRtcp(unprotect_packet->data, &unprotect_packet->length) < 0) {
          return;
        }
      } else {
        if (srtp->unprotectRtp(unprotect_packet->data, &unprotect_packet->length) < 0) {
          return;
        }
      }
    } else {
      return;
    }

    if (length <= 0) {
      return;
    }
    if (auto listener = getTransportListener().lock()) {
      listener->onTransportData(unprotect_packet, this);
    }
  }
}

void DtlsTransport::onCandidate(const CandidateInfo &candidate, IceConnection *conn) {
  if (auto listener = getTransportListener().lock()) {
    listener->onCandidate(candidate, this);
  }
}

void DtlsTransport::write(char* data, int len) {
  if (ice_ == nullptr || !running_) {
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
      if (srtp && ice_->checkIceState() == IceState::READY) {
        if (srtp->protectRtcp(protectBuf_, &length) < 0) {
          return;
        }
      }
    } else {
      comp = 1;

      if (srtp && ice_->checkIceState() == IceState::READY) {
        if (srtp->protectRtp(protectBuf_, &length) < 0) {
          return;
        }
      }
    }
    if (length <= 10) {
      return;
    }
    if (ice_->checkIceState() == IceState::READY) {
      writeOnIce(comp, protectBuf_, length);
    }
  }
}

void DtlsTransport::onDtlsPacket(DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) {
  bool is_rtcp = ctx == dtlsRtcp.get();
  int component_id = is_rtcp ? 2 : 1;

  packetPtr packet = std::make_shared<DataPacket>(component_id, data, len);

  if (is_rtcp) {
    writeDtlsPacket(dtlsRtcp.get(), packet);
  } else {
    writeDtlsPacket(dtlsRtp.get(), packet);
  }

  ELOG_DEBUG("%s message: Sending DTLS message, transportName: %s, componentId: %d",
             toLog(), transport_name.c_str(), packet->comp);
}

void DtlsTransport::writeDtlsPacket(DtlsSocketContext *ctx, packetPtr packet) {
  char data[1500];
  unsigned int len = packet->length;
  memcpy(data, packet->data, len);
  writeOnIce(packet->comp, data, len);
}

void DtlsTransport::onHandshakeCompleted(DtlsSocketContext *ctx, std::string clientKey, std::string serverKey,
                                         std::string srtp_profile) {
  boost::mutex::scoped_lock lock(sessionMutex_);
  std::string temp;

  if (rtp_timeout_checker_) {
    rtp_timeout_checker_->cancel();
  }
  if (rtcp_timeout_checker_) {
    rtcp_timeout_checker_->cancel();
  }

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

void DtlsTransport::onHandshakeFailed(DtlsSocketContext *ctx, const std::string& error) {
  ELOG_WARN("%s message: Handshake failed, transportName:%s, openSSLerror: %s",
            toLog(), transport_name.c_str(), error.c_str());
  running_ = false;
  updateTransportState(TRANSPORT_FAILED);
}

std::string DtlsTransport::getMyFingerprint() const {
  return dtlsRtp->getFingerprint();
}

void DtlsTransport::updateIceState(IceState state, IceConnection *conn) {
  std::weak_ptr<Transport> weak_transport = Transport::shared_from_this();
  worker_->task([weak_transport, state, conn, this]() {
    if (auto transport = weak_transport.lock()) {
      updateIceStateSync(state, conn);
    }
  });
}

void DtlsTransport::updateIceStateSync(IceState state, IceConnection *conn) {
  if (!running_) {
    return;
  }
  ELOG_DEBUG("%s message:IceState, transportName: %s, state: %d, isBundle: %d",
             toLog(), transport_name.c_str(), state, bundle_);
  if (state == IceState::INITIAL && this->getTransportState() != TRANSPORT_STARTED) {
    updateTransportState(TRANSPORT_STARTED);
  } else if (state == IceState::CANDIDATES_RECEIVED && this->getTransportState() != TRANSPORT_GATHERED) {
    updateTransportState(TRANSPORT_GATHERED);
  } else if (state == IceState::FAILED) {
    ELOG_DEBUG("%s message: Ice Failed", toLog());
    running_ = false;
    updateTransportState(TRANSPORT_FAILED);
  } else if (state == IceState::READY) {
    if (!isServer_ && dtlsRtp && !dtlsRtp->started) {
      ELOG_INFO("%s message: DTLSRTP Start, transportName: %s", toLog(), transport_name.c_str());
      dtlsRtp->start();
      rtp_timeout_checker_->scheduleCheck();
    }
    if (!isServer_ && dtlsRtcp != NULL && !dtlsRtcp->started) {
      ELOG_DEBUG("%s message: DTLSRTCP Start, transportName: %s", toLog(), transport_name.c_str());
      dtlsRtcp->start();
      rtcp_timeout_checker_->scheduleCheck();
    }
  }
}

void DtlsTransport::processLocalSdp(SdpInfo *localSdp_) {
  ELOG_DEBUG("%s message: processing local sdp, transportName: %s", toLog(), transport_name.c_str());
  localSdp_->isFingerprint = true;
  localSdp_->fingerprint = getMyFingerprint();
  std::string username(ice_->getLocalUsername());
  std::string password(ice_->getLocalPassword());
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
