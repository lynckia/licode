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
/**
 * A SRTP data Channel.
 * Represents a SRTP Channel with keys for protecting and unprotecting RTP and RTCP data.
 */
class SrtpChannel {

public:
	/**
	 * The constructor. At this point the class is only initialized but it still needs the Key pair.
	 */
	SrtpChannel();
	virtual ~SrtpChannel();
	/**
	 * Protects RTP Data
	 * @param buffer Pointer to the buffer with the data. The protected data is returned here
	 * @param len Pointer to the length of the data. The length is returned here
	 * @return 0 or an error code
	 */
	int protectRtp(char* buffer, int *len);
	/**
	 * Unprotects RTP Data
	 * @param buffer Pointer to the buffer with the data. The unprotected data is returned here
	 * @param len Pointer to the length of the data. The length is returned here
	 * @return 0 or an error code
	 */
	int unprotectRtp(char* buffer, int *len);
	/**
	 * Protects RTCP Data
	 * @param buffer Pointer to the buffer with the data. The protected data is returned here
	 * @param len Pointer to the length of the data. The length is returned here
	 * @return 0 or an error code
	 */
	int protectRtcp(char* buffer, int *len);
	/**
	 * Unprotects RTCP Data
	 * @param buffer Pointer to the buffer with the data. The unprotected data is returned here
	 * @param len Pointer to the length of the data. The length is returned here
	 * @return 0 or an error code
	 */
	int unprotectRtcp(char* buffer, int *len);
	/**
	 * Sets a key pair for the RTP channel
	 * @param sendingKey The key for protecting data
	 * @param receivingKey The key for unprotecting data
	 * @return true if everything is ok
	 */
	bool setRtpParams(char* sendingKey, char* receivingKey);
	/**
	 * Sets a key pair for the RTCP channel
	 * @param sendingKey The key for protecting data
	 * @param receivingKey The key for unprotecting data
	 * @return true if everything is ok
	 */
	bool setRtcpParams(char* sendingKey, char* receivingKey);
	/**
	 * Generates a valid key and encodes it in Base64
	 * @return The new key
	 */
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
