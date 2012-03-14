/*
 * WebRTCConnection.cpp
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#include "WebRTCConnection.h"

WebRTCConnection::WebRTCConnection() {
	// TODO Auto-generated constructor stub

}

WebRTCConnection::~WebRTCConnection() {
	// TODO Auto-generated destructor stub
}

bool WebRTCConnection::init(){
	return true;
}
bool WebRTCConnection::setRemoteSDP(const char* sdp){
	return true;
}
const char* WebRTCConnection::getLocalSDP(){
	return "";
}

