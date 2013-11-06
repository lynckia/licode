#ifndef DTLSCONNECTION_H_
#define DTLSCONNECTION_H_

#include <string.h>
#include <boost/thread/mutex.hpp>
#include "dtls/DtlsSocket.h"
#include "WebRtcConnection.h"
#include "NiceConnection.h"
#include "Transport.h"
#include "logger.h"

namespace erizo {
  class SrtpChannel;
  class DtlsTransport : dtls::DtlsReceiver, public Transport {
    DECLARE_LOGGER();
    public:
    DtlsTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, TransportListener *transportListener, const std::string &stunServer, int stunPort, int minPort, int maxPort);
    virtual ~DtlsTransport();
    void connectionStateChanged(IceState newState);
    std::string getMyFingerprint();
    static bool isDtlsPacket(const char* buf, int len);
    void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice);
    void write(char* data, int len);
    void writeDtls(dtls::DtlsSocketContext *ctx, const unsigned char* data, unsigned int len);
    void onHandshakeCompleted(dtls::DtlsSocketContext *ctx, std::string clientKey, std::string serverKey, std::string srtp_profile);
    void updateIceState(IceState state, NiceConnection *conn);
    void processLocalSdp(SdpInfo *localSdp_);

    private:
    char* protectBuf_, *unprotectBuf_;
    dtls::DtlsSocketContext *dtlsRtp, *dtlsRtcp;
    boost::mutex writeMutex_, readMutex_;
    SrtpChannel *srtp_, *srtcp_;
    bool readyRtp, readyRtcp;
    bool bundle_;
    friend class Transport;
  };
}
#endif
