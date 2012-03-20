//============================================================================
// Name        : hsam.cpp
// Author      : Pedro Rodriguez
// Version     :
// Copyright   :
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <stdio.h>
#include <fstream>
#include "NiceConnection.h"
#include "WebRTCConnection.h"
#include "sdpinfo.h"
#include <nice/nice.h>
#include <srtp/srtp.h>

using namespace std;

int main() {

//	SDPInfo pepe;
//	pepe.initWithSDP(str);
//	cout << pepe.getSDP() << endl;

	WebRTCConnection pepe;
	printf("pWQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQepepepe\n");
	pepe.init();
	printf("Local SDP %s\n", pepe.getLocalSDP().c_str());

	printf("push remote sdp\n");
	getchar();
	std::ifstream t("/home/pedro/workspace/webRTC/MCU/prototype/sdptodo");
	std::string str((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());
	cout << str <<endl;
	pepe.setRemoteSDP(str);


	return 0;
}
