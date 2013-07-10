#ifndef DTLSCONNECTION_H_
#define DTLSCONNECTION_H_

#include <string.h>
#include <boost/thread/mutex.hpp>
#include "dtls/DtlsSocket.h"
#include "WebRtcConnection.h"
#include "NiceConnection.h"
#include "Transport.h"

namespace erizo {
	class SrtpChannel;
	class DtlsTransport : dtls::DtlsReceiver, public Transport {
		public:
			DtlsTransport(MediaType med, const std::string &transport_name, bool rtcp_mux, TransportListener *transportListener);
			~DtlsTransport();
			void connectionStateChanged(IceState newState);
			std::string getMyFingerprint();
			static bool isDtlsPacket(const char* buf, int len);
			void onNiceData(char* data, int len, NiceConnection* nice);
			void write(char* data, int len, int sinkSSRC);
			void writeDtls(const unsigned char* data, unsigned int len);
      		void onHandshakeCompleted(std::string clientKey, std::string serverKey, std::string srtp_profile);
      		void updateIceState(IceState state, NiceConnection *conn);
      		void processLocalSdp(SdpInfo *localSdp_);
      		void close();

		private:
			char* protectBuf_, *unprotectBuf_;
			dtls::DtlsSocketContext *dtlsClient;
			boost::mutex writeMutex_, readMutex_;
			SrtpChannel *srtp_;
			friend class Transport;
	};
}
#endif