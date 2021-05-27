/*
 * UnencryptedTransport.cpp
 */

#include "UnencryptedTransport.h"

#include <string>
#include <cstring>
#include <memory>

#include "rtp/RtpHeaders.h"
#include "./NicerConnection.h"

using erizo::UnencryptedTransport;

DEFINE_LOGGER(UnencryptedTransport, "UnencryptedTransport");

using std::memcpy;


UnencryptedTransport::UnencryptedTransport(MediaType med, const std::string &transport_name,
                            const std::string& connection_id, bool bundle, bool rtcp_mux,
                            std::weak_ptr<TransportListener> transport_listener,
                            const IceConfig& iceConfig, std::string username, std::string password,
                            bool isServer, std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker):
  Transport(med, transport_name, connection_id, bundle, rtcp_mux, transport_listener, iceConfig, worker, io_worker) {
    ELOG_DEBUG("%s message: constructor, transportName: %s, isBundle: %d", toLog(), transport_name.c_str(), bundle);

    int comps = rtcp_mux ? 1 : 2;
    iceConfig_.connection_id = connection_id_;
    iceConfig_.transport_name = transport_name;
    iceConfig_.media_type = med;
    iceConfig_.ice_components = comps;
    iceConfig_.username = username;
    iceConfig_.password = password;
    ice_ = NicerConnection::create(io_worker_, iceConfig_);

    ELOG_DEBUG("%s message: created", toLog());
  }

UnencryptedTransport::~UnencryptedTransport() {
  if (this->state_ != TRANSPORT_FINISHED) {
    this->close();
  }
}

void UnencryptedTransport::start() {
  ice_->setIceListener(shared_from_this());
  ice_->copyLogContextFrom(*this);
  ELOG_DEBUG("%s message: starting ice", toLog());
  ice_->start();
}

void UnencryptedTransport::close() {
  ELOG_DEBUG("%s message: closing", toLog());
  running_ = false;
  ice_->close();
  this->state_ = TRANSPORT_FINISHED;
  ELOG_DEBUG("%s message: closed", toLog());
}

void UnencryptedTransport::maybeRestartIce(std::string username, std::string password) {
  if (!running_ || !ice_) {
    return;
  }
  ice_->maybeRestartIce(username, password);
}

void UnencryptedTransport::onIceData(packetPtr packet) {
  if (!running_) {
    return;
  }
  int len = packet->length;
  char *data = packet->data;
  unsigned int component_id = packet->comp;

  int length = len;
  if (this->getTransportState() == TRANSPORT_READY) {
    std::shared_ptr<DataPacket> unprotect_packet = std::make_shared<DataPacket>(component_id,
      data, len, VIDEO_PACKET, packet->received_time_ms);

    if (length <= 0) {
      return;
    }
    if (auto listener = getTransportListener().lock()) {
      listener->onTransportData(unprotect_packet, this);
    }
  }
}

void UnencryptedTransport::onCandidate(const CandidateInfo &candidate, IceConnection *conn) {
  if (auto listener = getTransportListener().lock()) {
    listener->onCandidate(candidate, this);
  }
}

void UnencryptedTransport::write(char* data, int len) {
  if (ice_ == nullptr || !running_) {
    return;
  }
  int length = len;
  if (this->getTransportState() == TRANSPORT_READY) {
    memcpy(protectBuf_, data, len);
    if (length <= 10) {
      return;
    }
    int comp = 1;
    RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(protectBuf_);
    if (chead->isRtcp() && !rtcp_mux_) {
      comp = 2;
    }
    if (ice_->checkIceState() == IceState::READY) {
      writeOnIce(comp, protectBuf_, length);
    }
  }
}

void UnencryptedTransport::updateIceState(IceState state, IceConnection *conn) {
  std::weak_ptr<Transport> weak_transport = Transport::shared_from_this();
  worker_->task([weak_transport, state, conn, this]() {
    if (auto transport = weak_transport.lock()) {
      updateIceStateSync(state, conn);
    }
  });
}

void UnencryptedTransport::updateIceStateSync(IceState state, IceConnection *conn) {
  if (!running_) {
    return;
  }
  ELOG_DEBUG("%s message:IceState, transportName: %s, state: %d, isBundle: %d, transportState: %d",
             toLog(), transport_name.c_str(), state, bundle_, getTransportState());
  if (state == IceState::INITIAL && this->getTransportState() != TRANSPORT_STARTED) {
    updateTransportState(TRANSPORT_STARTED);
  } else if (state == IceState::CANDIDATES_RECEIVED && this->getTransportState() != TRANSPORT_GATHERED) {
    updateTransportState(TRANSPORT_GATHERED);
  } else if (state == IceState::FAILED) {
    ELOG_DEBUG("%s message: Ice Failed", toLog());
    running_ = false;
    updateTransportState(TRANSPORT_FAILED);
  } else if (state == IceState::READY) {
    updateTransportState(TRANSPORT_READY);
  }
}

void UnencryptedTransport::processLocalSdp(SdpInfo *localSdp_) {
  ELOG_DEBUG("%s message: processing local sdp, transportName: %s", toLog(), transport_name.c_str());
  localSdp_->isFingerprint = false;
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
