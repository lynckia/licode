#ifndef ERIZO_SRC_ERIZO_TRANSPORT_H_
#define ERIZO_SRC_ERIZO_TRANSPORT_H_

#include <string>
#include <vector>
#include <cstdio>
#include "NiceConnection.h"

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
  virtual void onTransportData(char* buf, int len, Transport *transport) = 0;
  virtual void queueData(int comp, const char* data, int len,
                         Transport *transport, packetType type, uint16_t seqNum = 0) = 0;
  virtual void updateState(TransportState state, Transport *transport) = 0;
  virtual void onCandidate(const CandidateInfo& cand, Transport *transport) = 0;
};

class Transport : public NiceConnectionListener {
 public:
  boost::shared_ptr<NiceConnection> nice_;
  MediaType mediaType;
  std::string transport_name;
  Transport(MediaType med, const std::string& transport_name, const std::string& connection_id, bool bundle,
      bool rtcp_mux, TransportListener *transportListener, const IceConfig& iceConfig) :
    mediaType(med), transport_name(transport_name), rtcp_mux_(rtcp_mux), transpListener_(transportListener),
    connection_id_(connection_id), state_(TRANSPORT_INITIAL), iceConfig_(iceConfig), bundle_(bundle) {}
  virtual ~Transport() {}
  virtual void updateIceState(IceState state, NiceConnection *conn) = 0;
  virtual void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice) = 0;
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
    nice_->sendData(comp, buf, len);
  }
  bool setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool isBundle) {
    return nice_->setRemoteCandidates(candidates, isBundle);
  }
  bool rtcp_mux_;

 private:
  TransportListener *transpListener_;

 protected:
  std::string connection_id_;
  TransportState state_;
  IceConfig iceConfig_;
  bool bundle_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_TRANSPORT_H_
