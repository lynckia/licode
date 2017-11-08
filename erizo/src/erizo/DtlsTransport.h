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
class Resender;
class DtlsTransport : dtls::DtlsReceiver, public Transport {
  DECLARE_LOGGER();

 public:
  DtlsTransport(MediaType med, const std::string& transport_name, const std::string& connection_id, bool bundle,
                bool rtcp_mux, std::weak_ptr<TransportListener> transport_listener, const IceConfig& iceConfig,
                std::string username, std::string password, bool isServer, std::shared_ptr<Worker> worker,
                std::shared_ptr<IOWorker> io_worker);
  virtual ~DtlsTransport();
  void connectionStateChanged(IceState newState);
  std::string getMyFingerprint();
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
  void onHandshakeFailed(dtls::DtlsSocketContext *ctx, const std::string error) override;
  void updateIceState(IceState state, IceConnection *conn) override;
  void processLocalSdp(SdpInfo *localSdp_) override;

  void updateIceStateSync(IceState state, IceConnection *conn);

 private:
  char protectBuf_[5000];
  std::shared_ptr<DataPacket> unprotect_packet_;
  boost::scoped_ptr<dtls::DtlsSocketContext> dtlsRtp, dtlsRtcp;
  boost::mutex writeMutex_, sessionMutex_;
  boost::scoped_ptr<SrtpChannel> srtp_, srtcp_;
  bool readyRtp, readyRtcp;
  bool isServer_;
  std::unique_ptr<Resender> rtcp_resender_, rtp_resender_;
  packetPtr p_;
};

class Resender {
  DECLARE_LOGGER();

  // These values follow recommendations from section 4.2.4.1 in https://tools.ietf.org/html/rfc4347
  const unsigned int kMaxResends = 6;
  const unsigned int kInitialSecsPerResend = 1;

 public:
  Resender(DtlsTransport* transport, dtls::DtlsSocketContext* ctx);
  virtual ~Resender();
  void scheduleResend(packetPtr packet);
  void cancel();

 private:
  void scheduleNext();
  void resend();

 private:
  DtlsTransport* transport_;
  dtls::DtlsSocketContext* socket_context_;
  packetPtr packet_;
  unsigned int resend_seconds_;
  unsigned int max_resends_;
  std::shared_ptr<ScheduledTaskReference> scheduled_task_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_DTLSTRANSPORT_H_
