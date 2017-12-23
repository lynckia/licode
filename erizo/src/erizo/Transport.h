#ifndef ERIZO_SRC_ERIZO_TRANSPORT_H_
#define ERIZO_SRC_ERIZO_TRANSPORT_H_

#include <string>
#include <vector>
#include <cstdio>
#include "IceConnection.h"
#include "thread/Worker.h"
#include "thread/IOWorker.h"
#include "./logger.h"

/**
 * States of Transport
 */
enum TransportState {
  TRANSPORT_INITIAL, TRANSPORT_STARTED, TRANSPORT_GATHERED, TRANSPORT_READY, TRANSPORT_FINISHED, TRANSPORT_FAILED
};

namespace erizo {
class Transport;

class TransportListener {
 public:
  virtual void onTransportData(std::shared_ptr<DataPacket> packet, Transport *transport) = 0;
  virtual void updateState(TransportState state, Transport *transport) = 0;
  virtual void onCandidate(const CandidateInfo& cand, Transport *transport) = 0;
};

class Transport : public std::enable_shared_from_this<Transport>, public IceConnectionListener, public LogContext {
 public:
  std::shared_ptr<IceConnection> ice_;
  MediaType mediaType;
  std::string transport_name;
  Transport(MediaType med, const std::string& transport_name, const std::string& connection_id, bool bundle,
      bool rtcp_mux, std::weak_ptr<TransportListener> transport_listener, const IceConfig& iceConfig,
      std::shared_ptr<Worker> worker, std::shared_ptr<IOWorker> io_worker) :
    mediaType(med), transport_name(transport_name), rtcp_mux_(rtcp_mux), transport_listener_(transport_listener),
    connection_id_(connection_id), state_(TRANSPORT_INITIAL), iceConfig_(iceConfig), bundle_(bundle),
    running_{true}, worker_{worker},  io_worker_{io_worker} {}
  virtual ~Transport() {}
  virtual void updateIceState(IceState state, IceConnection *conn) = 0;
  virtual void onIceData(packetPtr packet) = 0;
  virtual void onCandidate(const CandidateInfo &candidate, IceConnection *conn) = 0;
  virtual void write(char* data, int len) = 0;
  virtual void processLocalSdp(SdpInfo *localSdp_) = 0;
  virtual void start() = 0;
  virtual void close() = 0;
  virtual std::shared_ptr<IceConnection> getIceConnection() { return ice_; }
  void setTransportListener(std::weak_ptr<TransportListener> listener) {
    transport_listener_ = listener;
  }
  std::weak_ptr<TransportListener> getTransportListener() {
    return transport_listener_;
  }
  TransportState getTransportState() {
    return state_;
  }
  void updateTransportState(TransportState state) {
    if (state == state_) {
      return;
    }
    state_ = state;
    if (auto listener = getTransportListener().lock()) {
      listener->updateState(state, this);
    }
  }
  void writeOnIce(int comp, void* buf, int len) {
    if (!running_) {
      return;
    }
    ice_->sendData(comp, buf, len);
  }
  bool setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool isBundle) {
    return ice_->setRemoteCandidates(candidates, isBundle);
  }

  void onPacketReceived(packetPtr packet) {
    std::weak_ptr<Transport> weak_transport = Transport::shared_from_this();
    worker_->task([weak_transport, packet]() {
      if (auto this_ptr = weak_transport.lock()) {
        if (packet->length > 0) {
          this_ptr->onIceData(packet);
        }
        if (packet->length == -1) {
          this_ptr->running_ = false;
          return;
        }
      }
    });
  }

  bool rtcp_mux_;

  inline std::string toLog() {
    return "id: " + connection_id_ + ", " + printLogContext();
  }

  std::shared_ptr<Worker> getWorker() {
    return worker_;
  }

 private:
  std::weak_ptr<TransportListener> transport_listener_;

 protected:
  std::string connection_id_;
  TransportState state_;
  IceConfig iceConfig_;
  bool bundle_;
  bool running_;
  std::shared_ptr<Worker> worker_;
  std::shared_ptr<IOWorker> io_worker_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_TRANSPORT_H_
