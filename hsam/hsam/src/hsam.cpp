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
using namespace std;

int main() {
	std::ifstream t("/home/pedro/workspace/webRTC/MCU/prototype/sdptodo");
	std::string str((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());
	//cout << str <<endl;
//	SDPInfo pepe;
//	pepe.initWithSDP(str);
//	cout << pepe.getSDP() << endl;
	//NiceConnection nic("138.4.4.141","173.194.70.126");
	WebRTCConnection pepe;
	printf("pWQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQepepepe\n");
	pepe.init();
	getchar();



	return 0;
}
