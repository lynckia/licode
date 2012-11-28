/*
 * Observer.cpp
 */

#include <time.h>
#include <boost/regex.hpp>
#include "Observer.h"

Observer::Observer(std::string name, SDPReceiver *receiver) :
		pc_(new PC(name)), name_(name), receiver_(receiver) {
	this->init();
}

Observer::~Observer() {
}

void Observer::wait() {
	m_Thread_.join();
}

void Observer::init() {
	m_Thread_ = boost::thread(&Observer::start, this);
}

void Observer::start() {
	pc_->RegisterObserver(this);

	pc_->Connect(name_);
	printf("Connected\n");
	while (true) {
		pc_->OnHangingGetConnect();
		pc_->OnHangingGetRead();
		sleep(1);
	}
}

void Observer::processMessage(int peer_id, const std::string& message) {
	printf("Processing Message %d, %s", peer_id, message.c_str());
	printf("OFFER1\n");
	std::string roap = message;

	// 	Pillar el OffererId
	// 	Generar AnswererId
	if (name_ == "publisher") {
		if (!receiver_->createPublisher(peer_id))
			return;
	} else {
		if (!receiver_->createSubscriber(peer_id))
			return;
	}

	std::string sdp = receiver_->getLocalSDP(peer_id);
	std::string sdp2 = Observer::Match(roap, "^.*sdp\":\"(.*)\",.*$");
	Observer::Replace(sdp2, "\\\\r\\\\n", "\\n");
	printf("sdp OFFER!!!!!!!!!!!!\n%s\n", sdp2.c_str());
	receiver_->setRemoteSDP(peer_id, sdp2);

	Observer::Replace(sdp, "\n", "\\\\r\\\\n");
	std::string answererSessionId = "106";
//	std::string offererSessionId = Observer::Match(roap, "^.*offererSessionId\":(.{32,32}).*$");
	std::string offererSessionId = Observer::Match(roap,
			"^.*offererSessionId\":(...).*$");
	std::string answer1("\n{\n \"messageType\":\"ANSWER\",\n");
	printf("sdp ANSWEEEER!!!!!!! %s\n", sdp.c_str());
	answer1.append(" \"sdp\":\"").append(sdp).append("\",\n");
	answer1.append(" \"offererSessionId\":").append(offererSessionId).append(
			",\n");
	answer1.append(" \"answererSessionId\":").append(answererSessionId).append(
			",\n");
	answer1.append(" \"seq\" : 1\n}\n");
	pc_->SendToPeer(peer_id, answer1);

}

void Observer::OnSignedIn() {
}
void Observer::OnDisconnected() {
	pthread_exit(0);
}
void Observer::OnPeerConnected(int id, const std::string& name) {

}
void Observer::OnPeerDisconnected(int peer_id) {
	receiver_->peerDisconnected(peer_id);

}
void Observer::OnMessageFromPeer(int peer_id, const std::string& message) {
	printf("OnMessageFromPeer\n");
	printf("message : %s\n", message.c_str());
	std::string roap = message;
	if (roap.find("OFFER") != std::string::npos) {
		boost::thread theThread(&Observer::processMessage, this, peer_id,
				message);
	}
}
void Observer::OnMessageSent(int err) {

}

void Observer::Replace(std::string& text, const std::string& pattern,
		const std::string& replace) {
	boost::regex regex_pattern(pattern, boost::regex_constants::perl);
	text = boost::regex_replace(text, regex_pattern, replace);
}
std::string Observer::Match(const std::string& text,
		const std::string& pattern) {
	boost::regex regex_pattern(pattern);
	boost::cmatch what;
	boost::regex_match(text.c_str(), what, regex_pattern);
	return (std::string(what[1].first, what[1].second));
}
