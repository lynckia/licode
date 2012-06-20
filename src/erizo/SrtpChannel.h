/*
 * Srtpchannel.h
 */

#ifndef SRTPCHANNEL_H_
#define SRTPCHANNEL_H_

#include <string>
#include <netinet/in.h>
#include <srtp/srtp.h>

namespace erizo {

typedef struct {
	uint32_t cc :4;
	uint32_t extension :1;
	uint32_t padding :1;
	uint32_t version :2;
	uint32_t payloadtype :7;
	uint32_t marker :1;
	uint32_t seqnum :16;
	uint32_t timestamp;
	uint32_t ssrc;
} rtpheader;

typedef struct {
	uint32_t blockcount :5;
	uint32_t padding :1;
	uint32_t version :2;
	uint32_t packettype :8;
	uint32_t length :16;
	uint32_t ssrc;
} rtcpheader;

class SrtpChannel {

public:
	SrtpChannel();
	virtual ~SrtpChannel();
	int protectRtp(char* buffer, int *len);
	int unprotectRtp(char* buffer, int *len);
	int protectRtcp(char* buffer, int *len);
	int unprotectRtcp(char* buffer, int *len);
	bool setRtpParams(char* sendingKey, char* receivingKey);
	bool setRtcpParams(char* sendingKey, char* receivingKey);
	static std::string generateBase64Key();

private:
	enum TransmissionType {
		SENDING, RECEIVING
	};

	bool configureSrtpSession(srtp_t *session, const char* key,
			enum TransmissionType type);

	bool active_;
	srtp_t send_session_;
	srtp_t receive_session_;
	srtp_t rtcp_send_session_;
	srtp_t rtcp_receive_session_;
};

} /* namespace erizo */
#endif /* SRTPCHANNEL_H_ */
