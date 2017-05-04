/*
 * NicerConnection.cpp
 */

#include "NicerConnection.h"

// nICEr includes
extern "C" {
#include <nr_api.h>
#include <registry.h>
#include <async_timer.h>
#include <r_crc32.h>
#include <r_memory.h>
#include <ice_reg.h>
#include <ice_util.h>
#include <transport_addr.h>
#include <nr_crypto.h>
#include <nr_socket.h>
#include <nr_socket_local.h>
#include <nr_proxy_tunnel.h>
#include <stun_client_ctx.h>
#include <stun_reg.h>
#include <stun_server_ctx.h>
#include <stun_util.h>
#include <ice_codeword.h>
#include <ice_ctx.h>
#include <ice_candidate.h>
#include <ice_handler.h>
}

#include <string>
#include <vector>

namespace erizo {

DEFINE_LOGGER(NicerConnection, "NicerConnection")

NicerConnection::NicerConnection(IceConnectionListener *listener, const IceConfig& ice_config)
    : IceConnection(listener, ice_config), ctx_{nullptr} {
}

NicerConnection::~NicerConnection() {
  close();
}

void NicerConnection::start() {
  std::string name = "agent";
  // Create the ICE context
  UINT4 flags = NR_ICE_CTX_FLAGS_AGGRESSIVE_NOMINATION;

  nr_ice_ctx_create(const_cast<char *>(name.c_str()), flags, &ctx_);
}

bool NicerConnection::setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool is_bundle) {
  return true;
}

void NicerConnection::gatheringDone(uint stream_id) {
}

void NicerConnection::getCandidate(uint stream_id, uint component_id, const std::string &foundation) {
}

void NicerConnection::setRemoteCredentials(const std::string& username, const std::string& password) {
}

int NicerConnection::sendData(unsigned int compId, const void* buf, int len) {
  return 0;
}

void NicerConnection::updateIceState(IceState state) {
}

IceState NicerConnection::checkIceState() {
  return IceState::READY;
}

void NicerConnection::updateComponentState(unsigned int compId, IceState state) {
}

void NicerConnection::onData(unsigned int component_id, char* buf, int len) {
}

CandidatePair NicerConnection::getSelectedPair() {
  return CandidatePair{};
}

void NicerConnection::setReceivedLastCandidate(bool hasReceived) {
}

void NicerConnection::close() {
}


IceConnection* NicerConnection::create(IceConnectionListener *listener, const IceConfig& ice_config) {
  return new NicerConnection(listener, ice_config);
}

} /* namespace erizo */
