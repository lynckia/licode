#ifndef TRANSPORT_H_
#define TRANSPORT_H_

#include <string.h>
#include <cstdio>
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
			virtual void onTransportData(char* buf, int len, Transport *transport) = 0;
			virtual void queueData(int comp, const char* data, int len, Transport *transport) = 0;
			virtual void updateState(TransportState state, Transport *transport) = 0;
	};
	class Transport : public NiceConnectionListener {
		public:
			NiceConnection *nice_;
			MediaType mediaType;
			Transport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, TransportListener *transportListener) {
				this->mediaType = med;
				transpListener_ = transportListener;
				rtcp_mux_ = rtcp_mux;
			}
			virtual void updateIceState(IceState state, NiceConnection *conn) = 0;
			virtual void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice) = 0;
			virtual void write(char* data, int len) = 0;
			virtual void processLocalSdp(SdpInfo *localSdp_) = 0;
			virtual void close()=0;
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
				state_ = state;
				if (transpListener_ != NULL) {
					transpListener_->updateState(state, this);
				}
			}
			NiceConnection* getNiceConnection() {
				return nice_;
			}
			void writeOnNice(int comp, void* buf, int len) {
				getNiceConnection()->sendData(comp, buf, len);
			}
			bool setRemoteCandidates(std::vector<CandidateInfo> &candidates) {
				return getNiceConnection()->setRemoteCandidates(candidates);
			}
			bool rtcp_mux_;
		private:
			TransportListener *transpListener_;

			TransportState state_;

			friend class NiceConnection;
	};
};
#endif