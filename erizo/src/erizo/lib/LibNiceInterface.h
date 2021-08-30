/*
 * LibNiceInterface.h
 */

#ifndef ERIZO_SRC_ERIZO_LIB_LIBNICEINTERFACE_H_
#define ERIZO_SRC_ERIZO_LIB_LIBNICEINTERFACE_H_



typedef struct _NiceAgent NiceAgent;
typedef struct _GMainContext GMainContext;
typedef struct _GMainLoop GMainLoop;
typedef struct _NiceCandidate NiceCandidate;
typedef struct _GSList GSList;

namespace erizo {

class LibNiceInterface {
 public:
  virtual NiceAgent* NiceAgentNew(GMainContext* context) = 0;
  virtual ~LibNiceInterface() {}
  virtual char* NiceInterfacesGetIpForInterface(const char *interface_name) = 0;
  virtual int NiceAgentAddStream(NiceAgent* agent, unsigned int n_components) = 0;
  virtual bool NiceAgentGetLocalCredentials(NiceAgent* agent, unsigned int stream_id,
      char** ufrag, char** pass) = 0;
  virtual bool NiceAgentAddLocalAddress(NiceAgent* agent, const char *ip) = 0;
  virtual void NiceAgentSetPortRange(NiceAgent* agent, unsigned int stream_id,
      unsigned int component_id, unsigned int min_port, unsigned int max_port) = 0;
  virtual bool NiceAgentGetSelectedPair(NiceAgent* agent, unsigned int stream_id,
    unsigned int component_id, NiceCandidate** local, NiceCandidate** remote) = 0;
  virtual bool NiceAgentSetRelayInfo(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      const char* server_ip, unsigned int server_port, const char* username, const char* password) = 0;
  virtual bool NiceAgentAttachRecv(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      GMainContext* ctx, void* func, void* data) = 0;
  virtual bool NiceAgentGatherCandidates(NiceAgent* agent, unsigned int stream_id) = 0;
  virtual bool NiceAgentSetRemoteCredentials(NiceAgent* agent, unsigned int stream_id, const char* ufrag,
      const char* pass) = 0;
  virtual int NiceAgentSetRemoteCandidates(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      const GSList* candidates) = 0;
  virtual GSList* NiceAgentGetLocalCandidates(NiceAgent* agent, unsigned int stream_id, unsigned int component_id) = 0;
  virtual int NiceAgentSend(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      unsigned int len, const char* buf) = 0;
};


class LibNiceInterfaceImpl: public LibNiceInterface {
 public:
  NiceAgent* NiceAgentNew(GMainContext* context);
  int NiceAgentAddStream(NiceAgent* agent, unsigned int n_components);
  char* NiceInterfacesGetIpForInterface(const char *interface_name);
  bool NiceAgentGetLocalCredentials(NiceAgent* agent, unsigned int stream_id,
      char** ufrag, char** pass);
  bool NiceAgentAddLocalAddress(NiceAgent* agent, const char *ip);
  void NiceAgentSetPortRange(NiceAgent* agent, unsigned int stream_id,
      unsigned int component_id, unsigned int min_port, unsigned int max_port);
  bool NiceAgentGetSelectedPair(NiceAgent* agent, unsigned int stream_id,
    unsigned int component_id, NiceCandidate** local, NiceCandidate** remote);
  bool NiceAgentSetRelayInfo(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      const char* server_ip, unsigned int server_port, const char* username, const char* password);
  bool NiceAgentAttachRecv(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      GMainContext* ctx, void* func, void* data);
  bool NiceAgentGatherCandidates(NiceAgent* agent, unsigned int stream_id);
  bool NiceAgentSetRemoteCredentials(NiceAgent* agent, unsigned int stream_id, const char* ufrag,
      const char* pass);
  int NiceAgentSetRemoteCandidates(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      const GSList* candidates);
  GSList* NiceAgentGetLocalCandidates(NiceAgent* agent, unsigned int stream_id, unsigned int component_id);
  int NiceAgentSend(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      unsigned int len, const char* buf);
};

}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_LIB_LIBNICEINTERFACE_H_
