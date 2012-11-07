/*
 * SDPProcessor.h
 */

#ifndef SDPINFO_H_
#define SDPINFO_H_

#include <string>
#include <vector>

namespace erizo {
/**
 * ICE candidate types
 */
enum HostType {
	HOST, SRLFX, PRFLX, RELAY
};
/**
 * Channel types
 */
enum MediaType {
	VIDEO_TYPE, AUDIO_TYPE, OTHER
};
/**
 * RTP Profile
 */
enum Profile {
  AVPF, SAVPF
};
/**
 * SRTP info.
 */
class CryptoInfo {
public:
	CryptoInfo() :
			tag(0) {
	}
	/**
	 * tag number
	 */
	int tag;
	/**
	 * The cipher suite. Only AES_CM_128_HMAC_SHA1_80 is supported as of now.
	 */
	std::string cipherSuite;
	/**
	 * The key
	 */
	std::string keyParams;
	/**
	 * The MediaType
	 */
	MediaType mediaType;

};
/**
 * Contains the information of an ICE Candidate
 */
class CandidateInfo {
public:
	CandidateInfo() :
			tag(0) {
	}
	bool isBundle;
	int tag;
	unsigned int priority;
	unsigned int componentId;
	std::string foundation;
	std::string hostAddress;
	std::string relayAddress;
	int hostPort;
	int relayPort;
	std::string netProtocol;
	HostType hostType;
	std::string transProtocol;
	std::string username;
	std::string password;
	MediaType mediaType;
};
/**
 * A PT to Codec map
 */
struct RtpMap {
  unsigned int payloadType;
  std::string encodingName;
  unsigned int clockRate;
  MediaType type;
};
/**
 * Contains the information of a single SDP.
 * Used to parse and generate SDPs
 */
class SdpInfo {
public:
	/**
	 * Constructor
	 */
	SdpInfo();
	virtual ~SdpInfo();
	/**
	 * Inits the object with a given SDP.
	 * @param sdp An string with the SDP.
	 * @return true if success
	 */
	bool initWithSdp(const std::string& sdp);
	/**
	 * Adds a new candidate.
	 * @param info The CandidateInfo containing the new candidate
	 */
	void addCandidate(const CandidateInfo& info);
	/**
	 * Adds SRTP info.
	 * @param info The CryptoInfo containing the information.
	 */
	void addCrypto(const CryptoInfo& info);
	/**
	 * Gets the candidates.
	 * @return A vector containing the current candidates.
	 */
	std::vector<CandidateInfo>& getCandidateInfos();
	/**
	 * Gets the SRTP information.
	 * @return A vector containing the CryptoInfo objects with the SRTP information.
	 */
	std::vector<CryptoInfo>& getCryptoInfos();
  /**
   * Gets the payloadType information
   * @return A vector containing the PT-codec information
   */
  std::vector<RtpMap>& getPayloadInfos();
	/**
	 * Gets the actual SDP.
	 * @return The SDP in string format.
	 */
	std::string getSdp();
	/**
	 * The audio and video SSRCs for this particular SDP.
	 */
	unsigned int audioSsrc, videoSsrc;
  /**
   * Is it Bundle
   */
  bool isBundle;
  /**
   * RTP Profile type
   */
  Profile profile;


private:
	bool processSdp(const std::string& sdp);
	bool processCandidate(char** pieces, int size, MediaType mediaType);
	std::vector<CandidateInfo> candidateVector_;
	std::vector<CryptoInfo> cryptoVector_;
  std::vector<RtpMap> payloadVector_;
	std::string iceUsername_;
	std::string icePassword_;
};
}/* namespace erizo */
#endif /* SDPPROCESSOR_H_ */
