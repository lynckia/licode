#ifndef ERIZO_SRC_ERIZO_UNENCRYPTEDTRANSPORT_H_
#define ERIZO_SRC_ERIZO_UNENCRYPTEDTRANSPORT_H_


#include <string>
#include "./IceConnection.h"
#include "./Transport.h"
#include "./logger.h"

namespace erizo {
class SrtpChannel;
class TimeoutChecker;
class UnencryptedTransport : public Transport {
  DECLARE_LOGGER();

 public:
  UnencryptedTransport(MediaType med, const std::string& transport_name, const std::string& connection_id, bool bundle,
                bool rtcp_mux, std::weak_ptr<TransportListener> transport_listener, const IceConfig& iceConfig,
                std::string username, std::string password, bool isServer, std::shared_ptr<Worker> worker,
                std::shared_ptr<IOWorker> io_worker);
  virtual ~UnencryptedTransport();
  void connectionStateChanged(IceState newState);
  void start() override;
  void close() override;
  void maybeRestartIce(std::string username, std::string password) override;
  void onIceData(packetPtr packet) override;
  void onCandidate(const CandidateInfo &candidate, IceConnection *conn) override;
  void write(char* data, int len) override;
  void updateIceState(IceState state, IceConnection *conn) override;
  void processLocalSdp(SdpInfo *localSdp_) override;

  void updateIceStateSync(IceState state, IceConnection *conn);

 private:
  char protectBuf_[5000];
  packetPtr p_;
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_UNENCRYPTEDTRANSPORT_H_
