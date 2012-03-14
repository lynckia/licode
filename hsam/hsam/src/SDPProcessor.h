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

struct CryptoParams {
  CryptoParams() : tag(0) {}
  CryptoParams(int t, const std::string& cs,
               const std::string& kp, const std::string& sp)
      : tag(t), cipher_suite(cs), key_params(kp), session_params(sp) {}

  bool Matches(const CryptoParams& params) const {
    return (tag == params.tag && cipher_suite == params.cipher_suite);
  }

  int tag;
  std::string cipher_suite;
  std::string key_params;
  std::string session_params;
};

struct CandidateInfo{
	CandidateInfo() : tag(0){}
	int tag;
	int priority;
	std::string foundation;
	std::string address;
	int port;
	std::string net_prot;
	hostType type;
	std::string name;
	std::string trans_prot;
	std::string net_name;
	std::string username;
	std::string passwd;
};

class SDPProcessor {
public:
	SDPProcessor();
	virtual ~SDPProcessor();
	bool initWithSDP(const std::string &sdp);
	const std::string &getSDP();

private:
	bool processSDP(const std::string &sdp);


};

#endif /* SDPPROCESSOR_H_ */
