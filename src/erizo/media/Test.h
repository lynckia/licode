#include <iostream>
#include <stdio.h>
#include <fstream>

#include "MediaProcessor.h"

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/regex.hpp>

#ifndef TEST_H_
#define TEST_H_

class Test {
public:
	Test();
	virtual ~Test();

	void rec();
	void send(char *buff, int buffSize);

	//UDPSocket *sock;
	MediaProcessor *mp;
};

#endif /* TEST_H_ */
