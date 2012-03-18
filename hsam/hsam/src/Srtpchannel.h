/*
 * Srtpchannel.h
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#ifndef SRTPCHANNEL_H_
#define SRTPCHANNEL_H_



struct srtp_event_data_t;
struct srtp_ctx_t;
typedef srtp_ctx_t* srtp_t;
struct srtp_policy_t;


class SrtpChannel {
public:
	int ProtectRtp(void* buffer, int len);
	int UnprotectRtp(void* buffer, int len);
	int ProtectRtcp(void* buffer, int len);
	int UnprotectRtcp(void* buffer, int len);
	bool SetRtpParams(char* sendingKey, char* receivingKey);
	bool SetRtcpParams(char* sendingKey, char* receivingKey);

	SrtpChannel();
	virtual ~SrtpChannel();
private:
	enum Type{
		SENDING,
		RECEIVING
	};
	srtp_t *send_session;
	srtp_t *receive_session;
	srtp_t *rtcp_send_session;
	srtp_t *rtcp_receive_session;
	bool configureSRTPsession(srtp_t *session, const char* key, enum Type type );
};

#endif /* SRTPCHANNEL_H_ */
