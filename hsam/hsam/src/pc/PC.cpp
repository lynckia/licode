#include "PC.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <stdio.h>
#include <cstdlib>

string servAddress = "hpcm.dit.upm.es";
int echoServPort = 8081;

const char kByeMessage[] = "BYE";

TCPSocket* CreateClientSocket() {
    TCPSocket *sock = new TCPSocket(servAddress, echoServPort);
    return sock;
}


PC::PC()
    :callback_(NULL),
    control_socket_(CreateClientSocket()),
    state_(NOT_CONNECTED),
    my_id_(-1),
    isSending(false) {
    
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

void PC::RegisterObserver(
    PCClientObserver* callback) {
  callback_ = callback;
}


bool PC::Connect(const std::string& client_name) {
  if (IsSendingMessage())
      return false;
  isSending = true;
  string signString = "GET /sign_in?" + client_name + " HTTP/1.0\r\n"+
        "Host:" + servAddress + " \r\n\r\n";
//  printf("Sending:\n%s", signString.c_str());
  const char * sign = signString.c_str();
  int signStringLen = strlen(sign);
  state_ = SIGNING_IN;


  control_socket_->send(sign, signStringLen);
  
  OnRead(control_socket_);
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

bool PC::ReadIntoBuffer(TCPSocket* socket,
        std::string* data,
        size_t* content_length) {
  char buffer[0xf];
  do {

    int bytes = socket->recv(buffer, sizeof(buffer));
    if (bytes <= 0)
      break;

    data->append(buffer, bytes);
  } while (true);
//  printf("Message received:\n%s", data->c_str());
  bool ret = false;
  size_t i = data->find("\r\n\r\n");
  if (i != std::string::npos) {
      size_t total_response_size;
    if (GetHeaderValue(*data, i, "\r\nContent-Length: ", content_length)) {
        total_response_size = (i + 4) + *content_length;
    } else {
        total_response_size = data->length();
        *content_length = data->length() - i - 4;
    }

    if (data->length() >= total_response_size) {
        ret = true;
        std::string should_close;
        const char kConnection[] = "\r\nConnection: ";
        if (GetHeaderValue(*data, i, kConnection, &should_close) &&
                                should_close.compare("close") == 0) {
            // socket->Close();
            // Since we closed the socket, there was no notification delivered
            // to us.  Compensate by letting ourselves know.
            OnClose(socket, 0);
        }
    } else {
        printf("We haven't received everything\n");
        // We haven't received everything.  Just continue to accept data.
    }
  }
 // printf("Content length: %lu, ret: %d\n", *content_length, ret);
  return ret;
}

void PC::OnRead(TCPSocket* socket) {
  size_t content_length = 0;
  if (ReadIntoBuffer(socket, &control_data_, &content_length)) {
    isSending = false;
    size_t peer_id = 0, eoh = 0;
//    printf("Response %s\n", control_data_.c_str());
    bool ok = ParseServerResponse(control_data_, content_length, &peer_id,
                                  &eoh);

    if (ok) {
      if (my_id_ == -1) {
        // First response.  Let's store our server assigned ID.
        my_id_ = peer_id;
        if (content_length) {
          size_t pos = eoh + 4;

          while (pos < control_data_.size()) {

            size_t eol = control_data_.find('\n', pos);
            if (eol == std::string::npos)
              break;
            int id = 0;
            std::string name;
            bool connected;

            if (ParseEntry(control_data_.substr(pos, eol - pos), &name, &id,
                           &connected) && id != my_id_) {
              peers_[id] = name;

              callback_->OnPeerConnected(id, name);

            }
            pos = eol + 1;
          }
        }

        callback_->OnSignedIn();
      } else if (state_ == SIGNING_OUT) {
        Close();
        callback_->OnDisconnected();
      } else if (state_ == SIGNING_OUT_WAITING) {
        SignOut();
      }
    } else {
        isSending = false;
    }

    control_data_.clear();

    if (state_ == SIGNING_IN) {
      state_ = CONNECTED;
    }
  }
}


bool PC::GetHeaderValue(const std::string& data,
                                          size_t eoh,
                                          const char* header_pattern,
                                          size_t* value) {
  size_t found = data.find(header_pattern);
  if (found != std::string::npos && found < eoh) {
    *value = atoi(&data[found + strlen(header_pattern)]);
    return true;
  }
  return false;
}

bool PC::GetHeaderValue(const std::string& data, size_t eoh,
                                          const char* header_pattern,
                                          std::string* value) {
  size_t found = data.find(header_pattern);
  if (found != std::string::npos && found < eoh) {
    size_t begin = found + strlen(header_pattern);
    size_t end = data.find("\r\n", begin);
    if (end == std::string::npos)
      end = eoh;
    value->assign(data.substr(begin, end - begin));
    return true;
  }
  return false;
}

bool PC::ParseEntry(const std::string& entry,
                                      std::string* name,
                                      int* id,
                                      bool* connected) {

  *connected = false;
  size_t separator = entry.find(',');
  if (separator != std::string::npos) {
    *id = atoi(&entry[separator + 1]);
    name->assign(entry.substr(0, separator));
    separator = entry.find(',', separator + 1);
    if (separator != std::string::npos) {
      *connected = atoi(&entry[separator + 1]) ? true : false;
    }
  }
  return !name->empty();
}

int PC::GetResponseStatus(const std::string& response) {
  int status = -1;
  size_t pos = response.find(' ');
  if (pos != std::string::npos)
    status = atoi(&response[pos + 1]);
  return status;
}

bool PC::ParseServerResponse(const std::string& response,
                                               size_t content_length,
                                               size_t* peer_id,
                                               size_t* eoh) {
  int status = GetResponseStatus(response.c_str());
  if (!(status == 200 || status == 304)) {
    Close();
    callback_->OnDisconnected();
    return false;
  }

  *eoh = response.find("\r\n\r\n");

  if (*eoh == std::string::npos)
    return false;

  *peer_id = -1;

  // See comment in peer_channel.cc for why we use the Pragma header and
  // not e.g. "X-Peer-Id".
  GetHeaderValue(response, *eoh, "\r\nPragma: ", peer_id);

  return true;
}

void PC::OnClose(TCPSocket* socket, int err) {
//  socket->Close();

#ifdef WIN32
  if (err != WSAECONNREFUSED) {
#else
  if (err != 100000) {
#endif
    if (socket == hanging_get_) {
      if (state_ == CONNECTED) {
//        hanging_get_->Close();
//        hanging_get_->Connect(server_address_);
      }
    } else {
      callback_->OnMessageSent(err);
    }
  } else {
    Close();
    callback_->OnDisconnected();
  }
}

void PC::Close() {
//  control_socket_->Close();
//  hanging_get_->Close();
  onconnect_data_.clear();
  peers_.clear();
  my_id_ = -1;
  state_ = NOT_CONNECTED;
}

void PC::OnHangingGetConnect() {
    if (IsSendingMessage())
        return;
    isSending = true;
    stringstream ss;
    ss << my_id_;
    string signString = "GET /wait?peer_id=" + ss.str() + " HTTP/1.0\r\n"
        "Host: " + servAddress + "\r\n\r\n";
    const char * sign = signString.c_str();
//    printf("Sending Wait...");
    int signStringLen = strlen(sign);
    hanging_get_ = CreateClientSocket();
    hanging_get_->send(sign, signStringLen);
//    printf("Sent\n");
}

void PC::OnHangingGetRead() {

  if (!IsSendingMessage())
      return;
  isSending = false;
  TCPSocket* socket = hanging_get_;

  size_t content_length = 0;
  //printf("Waiting...");
  if (ReadIntoBuffer(socket, &notification_data_, &content_length)) {
    size_t peer_id = 0, eoh = 0;
//    printf("Response %s\n", notification_data_.c_str());
    bool ok = ParseServerResponse(notification_data_, content_length,
                                  &peer_id, &eoh);
//printf("ok %d\n", ok);
    if (ok) {
      // Store the position where the body begins.
      size_t pos = eoh + 4;

      if (my_id_ == static_cast<int>(peer_id)) {
        // A notification about a new member or a member that just
        // disconnected.
        int id = 0;
        std::string name;
        bool connected = false;
        if (ParseEntry(notification_data_.substr(pos), &name, &id,
                       &connected)) {
          if (connected) {
            peers_[id] = name;
            callback_->OnPeerConnected(id, name);
          } else {
            peers_.erase(id);
            callback_->OnPeerDisconnected(id);
          }
        }
      } else {
//          printf("Notification: %s\n",notification_data_.substr(pos).c_str());
        OnMessageFromPeer(peer_id, notification_data_.substr(pos));
      }
    } 

  } else {
      isSending = false;
  }
  notification_data_.clear();
  printf("Done\n");

  if (state_ == CONNECTED) {
      OnHangingGetConnect();
  }
}

void PC::OnMessageFromPeer(int peer_id, const std::string& message) {
  if (message.length() == (sizeof(kByeMessage) - 1) &&
      message.compare(kByeMessage) == 0) {
    callback_->OnPeerDisconnected(peer_id);
  } else {
    std::string temp = std::string(message);
    callback_->OnMessageFromPeer(peer_id, temp);
  }
}

bool PC::SendToPeer(int peer_id, const std::string& message) {

	printf("SENDING TO %d \n %s\n", peer_id, message.c_str());
	if (state_ != CONNECTED)
		return false;

	if (!is_connected() || peer_id == -1)
		return false;

	stringstream ss;
	ss << my_id_;
	stringstream ss2;
	ss2 << peer_id;
	stringstream ss3;
	ss3 << message.length();
	string signString = "POST /message?peer_id=" + ss.str() + "&to=" + ss2.str() + " HTTP/1.0\r\n"
			+ "Host: " + servAddress + "\r\n"
			+ "Content-Length: "+ss3.str()+"\r\n"
			+ "Content-Type: text/plain\r\n"
			+  "\r\n" + message;

	const char * sign = signString.c_str();
	printf("Sending Message to %d \n", peer_id);
	int signStringLen = strlen(sign);
	control_socket_ = CreateClientSocket();
	control_socket_->send(sign, signStringLen);
	return true;
}

