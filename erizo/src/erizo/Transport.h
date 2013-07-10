#ifndef TRANSPORT_H_
#define TRANSPORT_H_

#include "NiceConnection.h"

/**
 * States of Transport
 */
enum TransportState {
	TRANSPORT_INITIAL, TRANSPORT_STARTED, TRANSPORT_READY, TRANSPORT_FINISHED, TRANSPORT_FAILED
};

namespace erizo {
	class Transport;
	class TransportListener {
		public:
			virtual void onTransportData(char* buf, int len, bool rtcp, unsigned int recvSSRC, Transport *transport) = 0;
			virtual void queueData(const char* data, int len) = 0;
			virtual void updateState(TransportState state, Transport *transport) = 0;
	};
	class Transport : public NiceConnectionListener {
		public:
			NiceConnection *nice_;
			MediaType mediaType;
			Transport(MediaType med, const std::string &transport_name, bool rtcp_mux, TransportListener *transportListener) {
				this->mediaType = mediaType;
				transpListener_ = transportListener;
			}
			virtual void updateIceState(IceState state, NiceConnection *conn) = 0;
			virtual void onNiceData(char* data, int len, NiceConnection* nice) = 0;
			virtual void write(char* data, int len, int sinkSSRC) = 0;
			virtual void processLocalSdp(SdpInfo *localSdp_) = 0;
			virtual void close()=0;
			void setTransportListener(TransportListener * listener) {
				printf("Setting transport listener to  %p\n", listener);
				transpListener_ = listener;
			}
			TransportListener* getTransportListener() {
				return transpListener_;
			}
			TransportState getTransportState() {
				return state_;
			}
			void updateTransportState(TransportState state) {
				printf("Updating transport state to %d\n", state);
				state_ = state;
				if (transpListener_ != NULL) {
					printf("Updating transport state to %p\n", transpListener_);
					transpListener_->updateState(state, this);
					printf("Updating transport state to %d\n", state);
				}
			}
			NiceConnection* getNiceConnection() {
				return nice_;
			}
			void writeOnNice(void* buf, int len) {
				getNiceConnection()->sendData(buf, len);
			}
			bool setRemoteCandidates(std::vector<CandidateInfo> &candidates) {
				return getNiceConnection()->setRemoteCandidates(candidates);
			}
		private:
			TransportListener *transpListener_;

			TransportState state_;

			friend class NiceConnection;
	};
};
#endif