#ifndef ERIZO_SRC_ERIZO_TRANSPORT_H_
#define ERIZO_SRC_ERIZO_TRANSPORT_H_

#include <string>
#include <vector>
#include <cstdio>
#include "NiceConnection.h"
#include "thread/Worker.h"
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
  virtual void onTransportData(std::shared_ptr<dataPacket> packet, Transport *transport) = 0;
  virtual void updateState(TransportState state, Transport *transport) = 0;
  virtual void onCandidate(const CandidateInfo& cand, Transport *transport) = 0;
};

class Transport : public std::enable_shared_from_this<Transport>, public NiceConnectionListener, public LogContext {
 public:
  boost::shared_ptr<NiceConnection> nice_;
  MediaType mediaType;
  std::string transport_name;
  Transport(MediaType med, const std::string& transport_name, const std::string& connection_id, bool bundle,
      bool rtcp_mux, TransportListener *transportListener, const IceConfig& iceConfig, std::shared_ptr<Worker> worker) :
    mediaType(med), transport_name(transport_name), rtcp_mux_(rtcp_mux), transpListener_(transportListener),
    connection_id_(connection_id), state_(TRANSPORT_INITIAL), iceConfig_(iceConfig), bundle_(bundle),
    running_{true}, worker_{worker} {}
  virtual ~Transport() {}
  virtual void updateIceState(IceState state, NiceConnection *conn) = 0;
  virtual void onNiceData(packetPtr packet) = 0;
  virtual void onCandidate(const CandidateInfo &candidate, NiceConnection *conn) = 0;
  virtual void write(char* data, int len) = 0;
  virtual void processLocalSdp(SdpInfo *localSdp_) = 0;
  virtual void start() = 0;
  virtual void close() = 0;
  virtual boost::shared_ptr<NiceConnection> getNiceConnection() { return nice_; }
  void setTransportListener(TransportListener * listener) {
    transpListener_ = listener;
  }
  TransportListener* getTransportListener() {
    return transpListener_;
  }
  TransportState getTransportState() {
    return state_;
  }
  void updateTransportState(TransportState state) {
    if (state == state_) {
      return;
    }
    state_ = state;
    if (transpListener_ != NULL) {
      transpListener_->updateState(state, this);
    }
  }
  void writeOnNice(int comp, void* buf, int len) {
    if (!running_) {
      return;
    }
    nice_->sendData(comp, buf, len);
  }
  bool setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool isBundle) {
    return nice_->setRemoteCandidates(candidates, isBundle);
  }

  void onPacketReceived(packetPtr packet) {
    std::weak_ptr<Transport> weak_transport = Transport::shared_from_this();
    worker_->task([weak_transport, packet]() {
      if (auto this_ptr = weak_transport.lock()) {
        if (packet->length > 0) {
          this_ptr->onNiceData(packet);
        }
        if (packet->length == -1) {
          this_ptr->running_ = false;
          return;
        }
      }
    });
  }

  bool rtcp_mux_;

  inline const char* toLog() {
    return ("id: " + connection_id_ + ", " + printLogContext()).c_str();
  }

  std::shared_ptr<Worker> getWorker() {
    return worker_;
  }

 private:
  TransportListener *transpListener_;

 protected:
  std::string connection_id_;
  TransportState state_;
  IceConfig iceConfig_;
  bool bundle_;
  bool running_;
  std::shared_ptr<Worker> worker_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_TRANSPORT_H_
