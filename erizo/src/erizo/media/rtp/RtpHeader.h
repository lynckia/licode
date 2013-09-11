/*
 * RtpHeader.h
 *
 *  Created on: Sep 20, 2012
 *      Author: pedro
 */

#ifndef RTPHEADER_H_
#define RTPHEADER_H_

class RTPHeader {
public:
	// constants

	/**
	 * KSize
	 * Longitud de la cabecera en bytes.
	 */
	static const int MIN_SIZE = 12;

public:
	// Constructor
	inline RTPHeader() :
			cc(0), extension(0), padding(0), version(2), payloadtype(0), marker(
					0), seqnum(0), timestamp(0), ssrc(0), extId(0), extLength(0) {
		// No implementation required
	}

public:
	// Member functions

	/**
	 * Get the marker bit from the RTP header.
	 * @return 1 if marker bit is set 0 if is not set.
	 */
	inline uint8_t getMarker() const {
		return marker;
	}

	/**
	 * Set the marker bit from the RTP header.
	 * @param aMarker 1 to set marker bit, 0 to unset it.
	 */
	inline void setMarker(uint8_t aMarker) {
		marker = aMarker;
	}

	/**
	 * Get the extension bit from the RTP header.
	 * @return 1 if extension bit is set 0 if is not set.
	 */
	inline uint8_t getExtension() const {
		return extension;
	}

	/**
	 * Set the extension bit from the RTP header
	 * @param ext 1 to set extension bit, 0 to unset i
	 */
	inline void setExtension(uint8_t ext) {
		extension = ext;
	}

	/**
	 * Get the payload type from the RTP header.
	 * @return A TInt8 holding the value.
	 */
	inline uint8_t getPayloadType() const {
		return payloadtype;
	}

	/**
	 * Set the payload type from the RTP header.
	 * @param aType the payload type. Valid range between 0x00 to 0x7F
	 */
	inline void setPayloadType(uint8_t aType) {
		payloadtype = aType;
	}

	/**
	 * Get the sequence number field from the RTP header.
	 * @return A TInt16 holding the value.
	 */
	inline uint16_t getSeqNumber() const {
		return ntohs(seqnum);
	}

	/**
	 * Set the seq number from the RTP header.
	 * @param aSeqNumber The seq number. Valid range between 0x0000 to 0xFFFF
	 */
	inline void setSeqNumber(uint16_t aSeqNumber) {
		seqnum = htons(aSeqNumber);
	}

	/**
	 * Get the Timestamp field from the RTP header.
	 * @return A TInt32 holding the value.
	 */
	inline uint32_t getTimestamp() const {
		return ntohl(timestamp);
	}

	/**
	 * Set the Timestamp from the RTP header.
	 * @param aTimestamp The Tmestamp. Valid range between 0x00000000 to 0xFFFFFFFF
	 */
	inline void setTimestamp(uint32_t aTimestamp) {
		timestamp = htonl(aTimestamp);
	}

	/**
	 * Get the SSRC field from the RTP header.
	 * @return A TInt32 holding the value.
	 */
	inline uint32_t getSSRC() const {
		return ntohl(ssrc);
	}

	/**
	 * Set the SSRC from the RTP header.
	 * @param aSSRC The SSRC. Valid range between 0x00000000 to 0xFFFFFFFF
	 */
	inline void setSSRC(uint32_t aSSRC) {
		ssrc = htonl(aSSRC);
	}
	/**
	 * Get the sequence number field from the RTP header.
	 * @return A TInt16 holding the value.
	 */
	inline uint16_t getExtId() const {
		return ntohs(extId);
	}

	/**
	 * Set the seq number from the RTP header.
	 * @param extensionId The seq number. Valid range between 0x0000 to 0xFFFF
	 */
	inline void setExtId(uint16_t extensionId) {
		extId = htons(extensionId);
	}

	/**
	 * Get the sequence number field from the RTP header.
	 * @return A TInt16 holding the value.
	 */
	inline uint16_t getExtLength() const {
		return ntohs(extLength);
	}

	/**
	 * Set the seq number from the RTP header.
	 * @param aSeqNumber The seq number. Valid range between 0x0000 to 0xFFFF
	 */
	inline void setExtLength(uint16_t extensionLength) {
		extId = htons(extensionLength);
	}

	/**
	 * Get the RTP header length
	 * @return the length in 8 bit units
	 */
	inline int getHeaderLength() {
		return MIN_SIZE + cc * 4 + extension * (4 + ntohs(extLength) * 4);
	}



private:
	// Data

	uint32_t cc :4;
	uint32_t extension :1;
	uint32_t padding :1;
	uint32_t version :2;
	uint32_t payloadtype :7;
	uint32_t marker :1;
	uint32_t seqnum :16;
	uint32_t timestamp;
	uint32_t ssrc;
	uint32_t extId :16;
	uint32_t extLength :16;

};

#endif /* RTPHEADER_H_ */
