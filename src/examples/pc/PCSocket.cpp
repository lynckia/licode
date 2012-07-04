/*
 * PCSocket.cpp
 */

#include "PCSocket.h"
#include <stdio.h>


#define PUBLISHER_PORT 8484
#define SUBSCRIBER_PORT 8485
std::string servAddress = "rocky.dit.upm.es"; //INSERT NODE SERVER ADDRESS HERE

using boost::asio::ip::tcp;

const char kByeMessage[] = "BYE";

PC::PC(const std::string &name) :
				callback_(NULL), state_(NOT_CONNECTED), my_id_(-1), isSending(false) {

	ioservice_ = new boost::asio::io_service;
	resolver_ = new tcp::resolver(*ioservice_);

	if (name.compare("publisher") == 0) {
		control_socket_ = CreateClientSocket(PUBLISHER_PORT);
	} else if (name.compare("subscriber") == 0) {
		control_socket_ = CreateClientSocket(SUBSCRIBER_PORT);
	}

}

PC::~PC() {
}

int PC::id() const {
	return my_id_;
}

bool PC::is_connected() const {
	return my_id_ != -1;
}

const Peers& PC::peers() const {
	return peers_;
}

void PC::RegisterObserver(PCClientObserver* callback) {
	callback_ = callback;
}

tcp::socket* PC::CreateClientSocket(int port) {
	//	tcp::socket *sock = new tcp::socket (servAddress, port);
	char portchar[6];
	sprintf(portchar, "%d", port);
	tcp::resolver::query query(tcp::v4(), servAddress, portchar);
	tcp::resolver::iterator iterator = resolver_->resolve(query);
	tcp::socket *sock = new tcp::socket(*ioservice_);
	boost::asio::connect(*sock, iterator);
	return sock;
}

bool PC::Connect(const std::string& client_name) {
	std::string signin = "SIGN_IN;";
	signin.append(client_name).append(";");
	//control_socket_->send(boost::asio::buffer(signin, signin.length() ), signin.length());
	boost::asio::write(*control_socket_,
			boost::asio::buffer((char*) signin.c_str(), signin.length()));
	state_ = CONNECTED;
	return true;
}

bool PC::SendHangUp(int peer_id) {
	return true;
}

bool PC::IsSendingMessage() {
	return (isSending);
}
bool PC::SignOut() {
	return true;
}

bool PC::ReadIntoBuffer(boost::asio::ip::tcp::socket* socket, std::string* data,
		size_t* content_length) {
	char charbuf[10000];

	size_t reply_length = control_socket_->read_some(
			boost::asio::buffer(charbuf, 10000));
	if (reply_length <= 0)
		state_ = NOT_CONNECTED;
	data->append(charbuf, reply_length);
	*content_length = strlen(data->c_str());
	printf("RECEIVED %s \n", data->c_str());
	parseMessage(data);
	return true;

}

void PC::OnRead(boost::asio::ip::tcp::socket* socket) {
	size_t content_length = 0;
	std::string the_data;
	if (state_ == CONNECTED) {
		if (ReadIntoBuffer(socket, &the_data, &content_length)) {
			//			printf("DATA\n%s\n", the_data.c_str());
		}
	}
}
void PC::OnClose(boost::asio::ip::tcp::socket* socket, int err) {

}

void PC::Close() {
	//  control_socket_->Close();
	//  hanging_get_->Close();

}

void PC::OnHangingGetConnect() {

}

void PC::OnHangingGetRead() {

	OnRead(control_socket_);

}

void PC::parseMessage(std::string *data) {
	size_t found1, found2;
	found1 = data->find(';');
	if (found1 == std::string::npos) {
		printf("Invalid Signalling Message\n");
		return;
	}
	std::string method = data->substr(0, found1);
	found2 = data->find(';', found1 + 1);
	if (found2 == std::string::npos) {
		printf("Invalid Signalling Message\n");
		return;
	}
	std::string id = data->substr(found1 + 1, found2 - (found1 + 1));
	std::string message = data->substr(found2 + 1, data->length());
	int the_id = atoi(id.c_str());
	if (method.compare("MSG_FROM_PEER") == 0)
		OnMessageFromPeer(the_id, message);
	if (method.compare("BYE") == 0) {
		callback_->OnPeerDisconnected(the_id);
	}

}

void PC::OnMessageFromPeer(int peer_id, const std::string& message) {
	callback_->OnMessageFromPeer(peer_id, message);

}

bool PC::SendToPeer(int peer_id, const std::string& message) {
	printf("SENDING TO %d \n %s\n", peer_id, message.c_str());
	char* peer[5];
	sprintf((char*) peer, "%d", peer_id);
	std::string msg;
	msg.append("MSG_TO_PEER;").append((char*) peer).append(";").append(message);
	//	control_socket_->send(msg.c_str(), msg.length());
	boost::asio::write(*control_socket_,
			boost::asio::buffer((char*) msg.c_str(), msg.length()));

	return true;
}

