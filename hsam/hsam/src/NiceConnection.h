/*
 * NiceConnection.h
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#ifndef NICECONNECTION_H_
#define NICECONNECTION_H_

#include <nice/nice.h>
#include <vector>
#include <string>
class NiceConnection {

public:
	NiceConnection();
	virtual ~NiceConnection();
	bool setRemoteCandidates(std::vector<std::string> &candidates);
	std::vector<std::string> &getLocalCandidates();
};

#endif /* NICECONNECTION_H_ */
