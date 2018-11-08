#ifndef ERIZO_SRC_ERIZO_DTLSTRANSPORT_H_
#define ERIZO_SRC_ERIZO_DTLSTRANSPORT_H_


#include <boost/thread/mutex.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include "dtls/DtlsSocket.h"
#include "./IceConnection.h"
#include "./Transport.h"
#include "./logger.h"

namespace erizo {
class SrtpChannel;
class TimeoutChecker;
class DtlsTransport : dtls::DtlsReceiver, public Transport {
  DECLARE_LOGGER();

 public:
  DtlsTransport(MediaType med, const std::string& transport_name, const std::string& connection_id, bool bundle,
                bool rtcp_mux, std::weak_ptr<TransportListener> transport_listener, const IceConfig& iceConfig,
                std::string username, std::string password, bool isServer, std::shared_ptr<Worker> worker,
                std::shared_ptr<IOWorker> io_worker);
  virtual ~DtlsTransport();
  void connectionStateChanged(IceState newState);
  std::string getMyFingerprint() const;
  static bool isDtlsPacket(const char* buf, int len);
  void start() override;
  void close() override;
  void onIceData(packetPtr packet) override;
  void onCandidate(const CandidateInfo &candidate, IceConnection *conn) override;
  void write(char* data, int len) override;
  void onDtlsPacket(dtls::DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) override;
  void writeDtlsPacket(dtls::DtlsSocketContext *ctx, packetPtr packet);
  void onHandshakeCompleted(dtls::DtlsSocketContext *ctx, std::string clientKey, std::string serverKey,
                            std::string srtp_profile) override;
  void onHandshakeFailed(dtls::DtlsSocketContext *ctx, const std::string& error) override;
  void updateIceState(IceState state, IceConnection *conn) override;
  void processLocalSdp(SdpInfo *localSdp_) override;

  void updateIceStateSync(IceState state, IceConnection *conn);

 private:
  char protectBuf_[5000];
  boost::scoped_ptr<dtls::DtlsSocketContext> dtlsRtp, dtlsRtcp;
  boost::mutex writeMutex_, sessionMutex_;
  boost::scoped_ptr<SrtpChannel> srtp_, srtcp_;
  bool readyRtp, readyRtcp;
  bool isServer_;
  std::unique_ptr<TimeoutChecker> rtcp_timeout_checker_, rtp_timeout_checker_;
  packetPtr p_;
};

class TimeoutChecker {
  DECLARE_LOGGER();

  const unsigned int kMaxTimeoutChecks = 15;
  const unsigned int kInitialSecsPerTimeoutCheck = 1;

 public:
  TimeoutChecker(DtlsTransport* transport, dtls::DtlsSocketContext* ctx);
  virtual ~TimeoutChecker();
  void scheduleCheck();
  void cancel();

 private:
  void scheduleNext();
  void resend();

 private:
  DtlsTransport* transport_;
  dtls::DtlsSocketContext* socket_context_;
  unsigned int check_seconds_;
  unsigned int max_checks_;
  std::shared_ptr<ScheduledTaskReference> scheduled_task_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_DTLSTRANSPORT_H_
