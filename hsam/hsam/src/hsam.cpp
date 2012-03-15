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
#include "sdpinfo.h"
using namespace std;

int main() {
	cout << "!!!Hello World!!!" << endl; // prints !!!Hello World!!!


	std::ifstream t("/home/pedro/workspace/webRTC/MCU/prototype/sdp");
	std::string str((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());
	//cout << str <<endl;

	return 0;
}
