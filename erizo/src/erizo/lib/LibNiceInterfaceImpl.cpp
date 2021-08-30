/*
 * LibNiceInterfaceImpl.cpp
 */

#include "./LibNiceInterface.h"
#include <nice/nice.h>
#include <nice/interfaces.h>

namespace erizo {

  NiceAgent* LibNiceInterfaceImpl::NiceAgentNew(GMainContext* context) {
    return nice_agent_new(context, NICE_COMPATIBILITY_RFC5245);
  }
  char* LibNiceInterfaceImpl::NiceInterfacesGetIpForInterface(const char *interface_name) {
    return nice_interfaces_get_ip_for_interface(const_cast<char*>(interface_name));
  }
  int LibNiceInterfaceImpl::NiceAgentAddStream(NiceAgent* agent, unsigned int n_components) {
    return nice_agent_add_stream(agent, n_components);
  }
  bool LibNiceInterfaceImpl::NiceAgentGetLocalCredentials(NiceAgent* agent, unsigned int stream_id,
      char** ufrag, char** pass) {
    return nice_agent_get_local_credentials(agent, stream_id, ufrag, pass);
  }
  void LibNiceInterfaceImpl::NiceAgentSetPortRange(NiceAgent* agent, unsigned int stream_id,
      unsigned int component_id, unsigned int min_port, unsigned int max_port) {
    return nice_agent_set_port_range(agent, stream_id, component_id, min_port, max_port);
  }
  bool LibNiceInterfaceImpl::NiceAgentAddLocalAddress(NiceAgent* agent, const char *ip) {
    NiceAddress addr;
    nice_address_init(&addr);
    nice_address_set_from_string(&addr, ip);
    return nice_agent_add_local_address(agent, &addr);
  }
  bool LibNiceInterfaceImpl::NiceAgentGetSelectedPair(NiceAgent* agent, unsigned int stream_id,
      unsigned int component_id, NiceCandidate** local, NiceCandidate** remote) {
    return nice_agent_get_selected_pair(agent, stream_id, component_id, local, remote);
  }
  bool LibNiceInterfaceImpl::NiceAgentSetRelayInfo(NiceAgent* agent, unsigned int stream_id,
      unsigned int component_id, const char* server_ip, unsigned int server_port, const char* username,
      const char* password) {
    return nice_agent_set_relay_info(agent, stream_id, component_id, server_ip, server_port, username,
        password, NICE_RELAY_TYPE_TURN_UDP);
  }
  bool LibNiceInterfaceImpl::NiceAgentAttachRecv(NiceAgent* agent, unsigned int stream_id,
      unsigned int component_id, GMainContext* ctx, void* func, void* data) {
    NiceAgentRecvFunc casted_function = reinterpret_cast<NiceAgentRecvFunc>(func);
    return nice_agent_attach_recv(agent, stream_id, component_id, ctx, casted_function, data);
  }
  bool LibNiceInterfaceImpl::NiceAgentGatherCandidates(NiceAgent* agent, unsigned int stream_id) {
    return nice_agent_gather_candidates(agent, stream_id);
  }
  bool LibNiceInterfaceImpl::NiceAgentSetRemoteCredentials(NiceAgent* agent, unsigned int stream_id,
      const char* ufrag, const char* pass) {
    return nice_agent_set_remote_credentials(agent, stream_id, ufrag, pass);
  }
  int LibNiceInterfaceImpl::NiceAgentSetRemoteCandidates(NiceAgent* agent, unsigned int stream_id,
      unsigned int component_id, const GSList* candidates) {
    return nice_agent_set_remote_candidates(agent, stream_id, component_id, candidates);
  }
  GSList* LibNiceInterfaceImpl::NiceAgentGetLocalCandidates(NiceAgent* agent, unsigned int stream_id,
      unsigned int component_id) {
    return nice_agent_get_local_candidates(agent, stream_id, component_id);
  }
  int LibNiceInterfaceImpl::NiceAgentSend(NiceAgent* agent, unsigned int stream_id, unsigned int component_id,
      unsigned int len, const char* buf) {
    return nice_agent_send(agent, stream_id, component_id, len, buf);
  }

}  // namespace erizo
