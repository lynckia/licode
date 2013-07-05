#ifndef DTLSCONNECTION_H_
#define DTLSCONNECTION_H_

#include <string.h>
#include "dtls/DtlsSocket.h"
#include "WebRtcConnection.h"

namespace erizo {
	class NiceConnection;
	class DtlsConnection : public WebRtcConnectionStateListener, public dtls::DtlsReceiver {
		public:
			DtlsConnection(NiceConnection *niceConnection);
			~DtlsConnection();
			void connectionStateChanged(IceState newState);
			std::string getMyFingerprint();
			static bool isDtlsPacket(const unsigned char* buf, int len);
			void read(const unsigned char* data, unsigned int len);
			void writeDtls(const unsigned char* data, unsigned int len);
      		void onHandshakeCompleted(std::string clientKey, std::string serverKey, std::string srtp_profile);
			/**
			 * Sets the associated WebRTCConnection.
			 * @param connection Pointer to the WebRtcConenction.
			 */
			void setWebRtcConnection(WebRtcConnection *connection);

		private:
			dtls::DtlsSocketContext *dtlsClient;
			NiceConnection *nice;
			WebRtcConnection* conn_;
	};
}
#endif