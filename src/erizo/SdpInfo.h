/*
 * SDPProcessor.h
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#ifndef SDPINFO_H_
#define SDPINFO_H_

#include <string>
#include <vector>

namespace erizo {

enum HostType {
	HOST, SRLFX, PRFLX, RELAY
};

enum MediaType {
	VIDEO_TYPE, AUDIO_TYPE, OTHER
};

class CryptoInfo {
public:
	CryptoInfo() :
			tag(0) {
	}
	int tag;
	std::string cipherSuite;
	std::string keyParams;
	//int ssrc;
	MediaType mediaType;

};

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

class SdpInfo {
public:
	SdpInfo();
	virtual ~SdpInfo();
	bool initWithSdp(const std::string& sdp);
	void addCandidate(const CandidateInfo& info);
	void addCrypto(const CryptoInfo& info);
	std::vector<CandidateInfo>& getCandidateInfos();
	std::vector<CryptoInfo>& getCryptoInfos();
	std::string getSdp();
	unsigned int audioSsrc, videoSsrc;

private:
	bool processSdp(const std::string& sdp);
	bool processCandidate(char** pieces, int size, MediaType mediaType);
	std::vector<CandidateInfo> candidateVector_;
	std::vector<CryptoInfo> cryptoVector_;
	std::string iceUsername_;
	std::string icePassword_;
};
}/* namespace erizo */
#endif /* SDPPROCESSOR_H_ */
