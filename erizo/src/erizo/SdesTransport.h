#ifndef SDESTRANSPORT_H_
#define SDESTRANSPORT_H_

#include <string.h>
#include "NiceConnection.h"
#include "Transport.h"
#include "logger.h"

namespace erizo {
  class SrtpChannel;
  class SdesTransport : public Transport {
    DECLARE_LOGGER();

    public:
    SdesTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, CryptoInfo *remoteCrypto, TransportListener *transportListener, const std::string &stunServer, int stunPort, int minPort, int maxPort);
    virtual ~SdesTransport();
    void connectionStateChanged(IceState newState);
    void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice);
    void write(char* data, int len);
    void updateIceState(IceState state, NiceConnection *conn);
    void processLocalSdp(SdpInfo *localSdp_);
    void setRemoteCrypto(CryptoInfo *remoteCrypto);
    
    private:
    char* protectBuf_, *unprotectBuf_;
    boost::mutex writeMutex_, readMutex_;
    SrtpChannel *srtp_, *srtcp_;
    bool readyRtp, readyRtcp;
    bool bundle_;
    CryptoInfo cryptoLocal_, cryptoRemote_;
  };
}
#endif
