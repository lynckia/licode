/*
 * SDPProcessor.h
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#ifndef SDPPROCESSOR_H_
#define SDPPROCESSOR_H_

#include <string>

enum hostType{
	HOST,
	SRLFX,
	PRFLX,
	RELAY
};

struct CryptoInfo {
  CryptoInfo() : tag(0) {}

  int tag;
  std::string cipher_suite;
  std::string key_params;
  std::string session_params;
};

struct CandidateInfo{
	CandidateInfo() : tag(0){}
	int tag;
	unsigned int priority;
	unsigned int compid;
	std::string foundation;
	std::string host_address;
	std::string relay_address;
	int host_port;
	int relay_port;
	std::string net_prot;
	hostType type;
	std::string name;
	std::string trans_prot;
	std::string net_name;
	std::string username;
	std::string passwd;
};

class SDPInfo {
public:
	SDPInfo();
	virtual ~SDPInfo();
	bool initWithSDP(const std::string &sdp);
	const std::string &getSDP();


private:
	bool processSDP(const std::string &sdp);
	void processCandidate (char** pieces, int size);



};

#endif /* SDPPROCESSOR_H_ */
