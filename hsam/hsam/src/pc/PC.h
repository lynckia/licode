#ifndef __PC_INCLUDED__
#define __PC_INCLUDED__

#include "PracticalSocket.h"

#include <map>

typedef std::map<int, std::string> Peers;

class PCClientObserver {
public:
  virtual void OnSignedIn() = 0;  // Called when we're logged on.
  virtual void OnDisconnected() = 0;
  virtual void OnPeerConnected(int id, const std::string& name) = 0;
  virtual void OnPeerDisconnected(int peer_id) = 0;
  virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;
  virtual void OnMessageSent(int err) = 0;
  virtual ~PCClientObserver() {}
};

class PC {
 public:
  enum State {
    NOT_CONNECTED,
    SIGNING_IN,
    CONNECTED,
    SIGNING_OUT_WAITING,
    SIGNING_OUT
  };

  PC();
  ~PC();

  int id() const;
  bool is_connected() const;
  const Peers& peers() const;

  void RegisterObserver(PCClientObserver* callback);

  bool Connect(const std::string& client_name);

  bool SendToPeer(int peer_id, const std::string& message);
  bool SendHangUp(int peer_id);
  bool IsSendingMessage();

  bool SignOut();
  void OnHangingGetRead();
  void OnHangingGetConnect();

 protected:
  void Close();
  bool ConnectControlSocket();
  void OnConnect(TCPSocket* socket);


  void OnMessageFromPeer(int peer_id, const std::string& message);

  // Quick and dirty support for parsing HTTP header values.
  bool GetHeaderValue(const std::string& data, size_t eoh,
                      const char* header_pattern, size_t* value);

  bool GetHeaderValue(const std::string& data, size_t eoh,
                      const char* header_pattern, std::string* value);

  // Returns true if the whole response has been read.
  bool ReadIntoBuffer(TCPSocket* socket, std::string* data,
                      size_t* content_length);

  void OnRead(TCPSocket* socket);

  void OnHangingGetRead(TCPSocket* socket);

  // Parses a single line entry in the form "<name>,<id>,<connected>"
  bool ParseEntry(const std::string& entry, std::string* name, int* id,
                  bool* connected);

  int GetResponseStatus(const std::string& response);

  bool ParseServerResponse(const std::string& response, size_t content_length,
                           size_t* peer_id, size_t* eoh);

  void OnClose(TCPSocket* socket, int err);

  PCClientObserver* callback_;
  std::string server_address_;
  TCPSocket* control_socket_;
  TCPSocket* hanging_get_;
  std::string onconnect_data_;
  std::string control_data_;
  std::string notification_data_;
  Peers peers_;
  State state_;
  int my_id_;
  bool isSending;
};


#endif