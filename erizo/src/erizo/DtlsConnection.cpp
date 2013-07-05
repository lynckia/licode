/*
 * DtlsConnection.cpp
 */
#include <iostream>
#include <cassert>

#include "DtlsConnection.h"

#include "NiceConnection.h"

 #include "dtls/DtlsFactory.h"

using namespace erizo;
using namespace std;
using namespace dtls;

DtlsConnection::DtlsConnection(NiceConnection *niceConnection) {
	cout << "Initializing DtlsConnection" << endl;
	nice = niceConnection;
	dtlsClient = new DtlsSocketContext();
	dtlsClient->setDtlsReceiver(this);
}

DtlsConnection::~DtlsConnection() {
}

void DtlsConnection::read(const unsigned char* data, unsigned int len) {
	dtlsClient->read(data, len);
}

void DtlsConnection::writeDtls(const unsigned char* data, unsigned int len) {
	nice->sendData(data, len);
}

void DtlsConnection::onHandshakeCompleted(std::string clientKey,std::string serverKey, std::string srtp_profile) {
	if (this->conn_ != NULL) {
		this->conn_->startSRTP(clientKey, serverKey, srtp_profile);
	}
}

std::string DtlsConnection::getMyFingerprint() {
	return dtlsClient->getFingerprint();
}

void DtlsConnection::connectionStateChanged(IceState newState) {
  if (newState == READY) {
  	dtlsClient->start();
  }
}

void DtlsConnection::setWebRtcConnection(WebRtcConnection* connection) {
	this->conn_ = connection;
}


bool DtlsConnection::isDtlsPacket(const unsigned char* buf, int len) {
	int data = DtlsFactory::demuxPacket(buf,len);
	//cout << "Checking DTLS: " << data << endl;
	switch(data)
    {
    case DtlsFactory::dtls:
       return true;
       break;
    default:
       return false;
       break;
    }
}