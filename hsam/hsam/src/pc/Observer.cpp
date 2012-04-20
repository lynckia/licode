#include "Observer.h"

#include <boost/regex.hpp>

//#include <iostream>           // For cerr and cout
#include <time.h>

Observer::Observer(std::string name, SDPReceiver *receiver):pc_(new PC(name)), name_(name), receiver_(receiver) {
    pthread_create(&thread, 0, &Observer::init, this);
}

Observer::~Observer() {
}

void Observer::wait() {
    pthread_join(thread, 0);
}

void *Observer::init(void *obj) {
    //All we do here is call the do_work() function
    reinterpret_cast<Observer *>(obj)->start();
    return NULL;
}

//void sleep ( int seconds )
//{
//  clock_t endwait;
//  endwait = clock () + seconds * CLOCKS_PER_SEC ;
//  while (clock() < endwait) {}
//}

void Observer::start() {
    pc_->RegisterObserver(this);
    
    pc_->Connect(name_);
    printf("Connected\n");
    while(true) {
        pc_->OnHangingGetConnect();
        pc_->OnHangingGetRead();
        sleep(3);
    }
}

void Observer::OnSignedIn(){
    
}
void Observer::OnDisconnected(){
    pthread_exit(0);
}
void Observer::OnPeerConnected(int id, const std::string& name){
    
}
void Observer::OnPeerDisconnected(int peer_id){
    
}
void Observer::OnMessageFromPeer(int peer_id, const std::string& message){
	printf("OnMessageFromPeer\n");
	printf("message : %s\n",message.c_str());
	std::string roap = message;
	if (roap.find("OFFER") != std::string::npos) {
		// OFFER1: Inicializo a Pedro y devuelvo el primer SDP como ANSWER1
		// Pasar a ROAP
		printf("OFFER1\n");

		// 	Pillar el OffererId
		// 	Generar AnswererId
		if (name_ == "publisher") {
			receiver_->createPublisher(peer_id);
	     } else {
	    	receiver_->createSubscriber(peer_id);
	     }

		std::string sdp = receiver_->getLocalSDP(peer_id);
		std::string sdp2 = Observer::Match(roap, "^.*sdp\" : \"(.*)\",\n.*$");
		Observer::Replace(sdp2,"\\\\r\\\\n","\\n");
		printf("sdp OFFER!!!!!!!!!!!!\n%s\n", sdp2.c_str());
		receiver_->setRemoteSDP(peer_id, sdp2);

		Observer::Replace(sdp, "a=ssrc[^\n]*\n", "");
		Observer::Replace(sdp, "\n", "\\\\r\\\\n");
		std::string answererSessionId = "FteaVmkG83fNDPWqhUpUERBYNu7z98fV";
		std::string offererSessionId = Observer::Match(roap, "^.*offererSessionId\" : \"(.{32,32}).*$");
		std::string answer1 ("SDP\n{\n \"messageType\" : \"ANSWER\",\n");
		answer1.append(" \"offererSessionId\" : \"").append(offererSessionId).append("\",\n");
		answer1.append(" \"answererSessionId\" : \"").append(answererSessionId).append("\",\n");
		printf("sdp ANSWEEEER!!!!!!! %s\n", sdp.c_str());
		answer1.append(" \"sdp\" : \"").append(sdp).append("\",\n");
		answer1.append(" \"seq\" : 1\n}\n");
		pc_->SendToPeer(peer_id, answer1);
	} else if (roap.find("ANSWER") != std::string::npos) {
		printf("ANSWER\n");
		// ANSWER2: Quiero pasar el SDP y devuelvo OK2
		// Obtener el SDP
		// Crear OK
		// 	Pillar el OffererId
		// 	Pillar el AnswererId
//		std::string sdp = Observer::Match(roap, "^.*sdp\" : \"(.*)\",\n.*$");
//		Observer::Replace(sdp,"\\\\r\\\\n","\\n");
//		printf("sdp\n%s\n", sdp.c_str());
//		receiver_->setRemoteSDP(peer_id, sdp);
		std::string offererSessionId = Observer::Match(roap, "^.*offererSessionId\" : \"(.{32,32}).*$");
		std::string answererSessionId = Observer::Match(roap, "^.*answererSessionId\" : \"(.{32,32}).*$");
		std::string ok2 ("SDP\n{\n \"messageType\" : \"OK\",\n");
		ok2.append(" \"offererSessionId\" : \"").append(answererSessionId).append("\",\n");
		ok2.append(" \"answererSessionId\" : \"").append(offererSessionId).append("\",\n");
		ok2.append(" \"seq\" : 2\n}\n");
		pc_->SendToPeer(peer_id, ok2);
	} else if (roap.find("OK") != std::string::npos) {
		printf("OK\n");
		// OK1: Solo devuelvo parte del ANSWER1 como OFFER2
		// Quitar candidatos
		// Pasar a ROAP
		// 	Pillar el OffererId
		// 	Pillar el AnswererId

		std::string sdp = receiver_->getLocalSDP(peer_id);
		std::string offererSessionId = Observer::Match(roap, "^.*offererSessionId\" : \"(.{32,32}).*$");
		std::string answererSessionId = Observer::Match(roap, "^.*answererSessionId\" : \"(.{32,32}).*$");
		Observer::Replace(sdp, "\\\\n", "\n");
		//Observer::Replace(sdp, "a=candidate[^\n]*generation 0\n", "");
		Observer::Replace(sdp, "\n", "\\\\r\\\\n");
		std::string offer2 ("SDP\n{\n \"messageType\" : \"OFFER\",\n");
		offer2.append(" \"offererSessionId\" : \"").append(offererSessionId).append("\",\n");
		offer2.append(" \"answererSessionId\" : \"").append(answererSessionId).append("\",\n");
		offer2.append(" \"sdp\" : \"").append(sdp).append("\",\n");
		offer2.append(" \"seq\" : 2,\n \"tieBreaker\" : 802577871\n}\n");
		pc_->SendToPeer(peer_id, offer2);
	}

}
void Observer::OnMessageSent(int err){
    
}

void Observer::Replace(std::string& text, const std::string& pattern, const std::string& replace) {
	boost::regex regex_pattern (pattern,boost::regex_constants::perl);

	text = boost::regex_replace (text, regex_pattern , replace);
}
std::string Observer::Match(const std::string& text, const std::string& pattern) {
	boost::regex regex_pattern (pattern);
	boost::cmatch what;
	boost::regex_match(text.c_str(), what, regex_pattern);
	return (std::string(what[1].first, what[1].second));
}
