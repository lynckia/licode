#ifndef ERIZO_SRC_ERIZO_DTLSTRANSPORT_H_
#define ERIZO_SRC_ERIZO_DTLSTRANSPORT_H_


#include <boost/thread/mutex.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include "dtls/DtlsSocket.h"
#include "./NiceConnection.h"
#include "./Transport.h"
#include "./logger.h"

namespace erizo {
class SrtpChannel;
class Resender;
class DtlsTransport : dtls::DtlsReceiver, public Transport {
  DECLARE_LOGGER();

 public:
  DtlsTransport(MediaType med, const std::string& transport_name, const std::string& connection_id, bool bundle,
                bool rtcp_mux, TransportListener *transportListener, const IceConfig& iceConfig, std::string username,
                std::string password, bool isServer);
  virtual ~DtlsTransport();
  void connectionStateChanged(IceState newState);
  std::string getMyFingerprint();
  static bool isDtlsPacket(const char* buf, int len);
  void start();
  void close();
  void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice);
  void onCandidate(const CandidateInfo &candidate, NiceConnection *conn);
  void write(char* data, int len);
  void onDtlsPacket(dtls::DtlsSocketContext *ctx, const unsigned char* data, unsigned int len);
  void writeDtlsPacket(dtls::DtlsSocketContext *ctx, const unsigned char* data, unsigned int len);
  void onHandshakeCompleted(dtls::DtlsSocketContext *ctx, std::string clientKey, std::string serverKey,
                            std::string srtp_profile);
  void onHandshakeFailed(dtls::DtlsSocketContext *ctx, const std::string error);
  void updateIceState(IceState state, NiceConnection *conn);
  void processLocalSdp(SdpInfo *localSdp_);

 private:
  char protectBuf_[5000];
  char unprotectBuf_[5000];
  boost::scoped_ptr<dtls::DtlsSocketContext> dtlsRtp, dtlsRtcp;
  boost::mutex writeMutex_, sessionMutex_;
  boost::scoped_ptr<SrtpChannel> srtp_, srtcp_;
  bool readyRtp, readyRtcp;
  bool running_, isServer_;
  boost::scoped_ptr<Resender> rtcp_resender_, rtp_resender_;
  boost::thread getNice_Thread_;
  packetPtr p_;

  const unsigned int kMaxResends = 5;
  const unsigned int kSecsPerResend = 3;

  void getNiceDataLoop();
  inline const char* toLog() {
    return (std::string("id: ")+connection_id_).c_str();
  }
};

class Resender {
  DECLARE_LOGGER();

 public:
  Resender(DtlsTransport* transport, dtls::DtlsSocketContext* ctx, unsigned int resend_seconds,
      unsigned int max_resends);
  virtual ~Resender();
  void ScheduleResend(const unsigned char* data, unsigned int len);
  void Run();
  void Cancel();
  void Resend(const boost::system::error_code& ec);

 private:
  DtlsTransport* transport_;
  dtls::DtlsSocketContext* socket_context_;
  unsigned char data_[1500];
  unsigned int len_;
  unsigned int resend_seconds_;
  unsigned int max_resends_;

  boost::asio::io_service service;
  boost::asio::deadline_timer timer;
  boost::scoped_ptr<boost::thread> thread_;
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_DTLSTRANSPORT_H_
