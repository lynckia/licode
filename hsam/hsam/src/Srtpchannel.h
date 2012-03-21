/*
 * Srtpchannel.h
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#ifndef SRTPCHANNEL_H_
#define SRTPCHANNEL_H_

#include <string>
#include <netinet/in.h>
#include <srtp/srtp.h>


//struct srtp_event_data_t;
//struct srtp_ctx_t;
//typedef srtp_ctx_t* srtp_t;
//struct srtp_policy_t;


typedef struct
{
	uint32_t cc:4;
	uint32_t extension:1;
	uint32_t padding:1;
	uint32_t version:2;
	uint32_t payloadtype:7;
	uint32_t marker:1;
	uint32_t seqnum:16;
	uint32_t timestamp;
	uint32_t ssrc;
}rtpheader;


typedef struct
{
	uint32_t blockcount:5;
	uint32_t padding:1;
	uint32_t version:2;
	uint32_t packettype:8;
	uint32_t length:16;
	uint32_t ssrc;
}rtcpheader;

class SrtpChannel {
public:
	int ProtectRtp(char* buffer, int *len);
	int UnprotectRtp(char* buffer, int *len);
	int ProtectRtcp(char* buffer, int *len);
	int UnprotectRtcp(char* buffer, int *len);
	bool SetRtpParams(char* sendingKey, char* receivingKey);
	bool SetRtcpParams(char* sendingKey, char* receivingKey);
	static std::string generateBase64Key();

	SrtpChannel();
	virtual ~SrtpChannel();
private:
	bool active;
	enum Type{
		SENDING,
		RECEIVING
	};
	srtp_t send_session;
	srtp_t receive_session;
	srtp_t rtcp_send_session;
	srtp_t rtcp_receive_session;
	bool configureSRTPsession(srtp_t *session, const char* key, enum Type type );
};

#endif /* SRTPCHANNEL_H_ */
