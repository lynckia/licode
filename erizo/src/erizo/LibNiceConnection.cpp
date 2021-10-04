/*
 * LibNiceConnection.cpp
 */

#include <cstdio>
#include <string>
#include <cstring>
#include <vector>

#include "LibNiceConnection.h"
#include "SdpInfo.h"
#include "lib/Clock.h"
#include "lib/ClockUtils.h"

using std::memcpy;

// If true (and configured properly below) erizo will generate relay candidates for itself
// MOSTLY USEFUL WHEN ERIZO ITSELF IS BEHIND A NAT
#define SERVER_SIDE_TURN 0

namespace erizo {

DEFINE_LOGGER(LibNiceConnection, "LibNiceConnection")

void LibNiceConnection::receive_message(NiceAgent* agent, guint stream_id, guint component_id,
    guint len, gchar* buf, gpointer user_data) {
  if (user_data == NULL || len == 0) {
    return;
  }
  packetPtr packet (new DataPacket());
  memcpy(packet->data, buf, len);
  packet->comp = component_id;
  packet->length = len;
  packet->received_time_ms = ClockUtils::timePointToMs(clock::now());
  LibNiceConnection* conn = reinterpret_cast<LibNiceConnection*>(user_data);
  conn->async([component_id, packet]
    (std::shared_ptr<LibNiceConnection> conn_ptr) {
      conn_ptr->onData(component_id, packet);
    });
}

void LibNiceConnection::new_local_candidate(NiceAgent *agent, guint stream_id, guint component_id, gchar *foundation,
    gpointer user_data) {
  LibNiceConnection *conn = reinterpret_cast<LibNiceConnection*>(user_data);
  std::string found(foundation);
  conn->async([stream_id, component_id, found]
    (std::shared_ptr<LibNiceConnection> conn_ptr) {
      conn_ptr->getCandidate(stream_id, component_id, found);
    });
}

void LibNiceConnection::candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer user_data) {
  LibNiceConnection *conn = reinterpret_cast<LibNiceConnection*>(user_data);
  conn->async([stream_id]
    (std::shared_ptr<LibNiceConnection> conn_ptr) {
      conn_ptr->gatheringDone(stream_id);
    });
}

void LibNiceConnection::component_state_change(NiceAgent *agent, guint stream_id,
    guint component_id, guint state, gpointer user_data) {
  LibNiceConnection *conn = reinterpret_cast<LibNiceConnection *>(user_data);
  conn->async([component_id, state]
    (std::shared_ptr<LibNiceConnection> conn_ptr) {
      if (state == NICE_COMPONENT_STATE_CONNECTED) {
      } else if (state == NICE_COMPONENT_STATE_FAILED) {
        conn_ptr->updateComponentState(component_id, IceState::FAILED);
      }
    });
}

void LibNiceConnection::new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
    NiceCandidate *lcandidate, NiceCandidate *rcandidate, gpointer user_data) {
  LibNiceConnection *conn = reinterpret_cast<LibNiceConnection*>(user_data);
  conn->async([component_id]
    (std::shared_ptr<LibNiceConnection> conn_ptr) {
      conn_ptr->updateComponentState(component_id, IceState::READY);
    });
}

std::string LibNiceConnection::getHostTypeFromCandidate(NiceCandidate *candidate) {
  switch (candidate->type) {
    case NICE_CANDIDATE_TYPE_HOST: return "host";
    case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE: return "serverReflexive";
    case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE: return "peerReflexive";
    case NICE_CANDIDATE_TYPE_RELAYED: return "relayed";
    default: return "unknown";
  }
}

LibNiceConnection::LibNiceConnection(std::shared_ptr<LibNiceInterface> libnice,
std::shared_ptr<IOWorker> io_worker,
const IceConfig& ice_config)
  : IceConnection{ice_config},
    lib_nice_{libnice}, io_worker_{io_worker},
    agent_{NULL}, cands_delivered_{0}, closed_{false},
    enable_ice_lite_{ice_config.ice_lite},
    stream_id_{0} {
  #if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
  #endif
}

LibNiceConnection::~LibNiceConnection() {
  if (!closed_) {
    ELOG_WARN("%s message: Destructor without a previous close", toLog());
  }
}

void LibNiceConnection::close() {
  if (closed_) {
    return;
  }
  auto shared_this = shared_from_this();
  async([shared_this] (std::shared_ptr<LibNiceConnection> this_ptr) {
    shared_this->closeSync();
  });
}

void LibNiceConnection::closeSync() {
  if (closed_) {
    return;
  }
  ELOG_DEBUG("%s message: closing", toLog());
  this->updateIceState(IceState::FINISHED);
  listener_.reset();
  if (agent_ != NULL) {
    forEachComponent([this] (uint comp_id) {
      ELOG_DEBUG("%s message: Detaching recv for comp_id %d", toLog(), comp_id);
      lib_nice_->NiceAgentAttachRecv(agent_, stream_id_, comp_id, io_worker_->getGlibContext(), NULL, NULL);
    });

    g_object_unref(agent_);
    agent_ = NULL;
  }
  closed_ = true;
  ELOG_DEBUG("%s message: closed", toLog());
}

void LibNiceConnection::onData(unsigned int component_id, packetPtr packet) {
  IceState state = this->checkIceState();
  if (state == IceState::READY) {
    if (auto listener = getIceListener().lock()) {
      listener->onPacketReceived(packet);
    }
  }
}

int LibNiceConnection::sendData(unsigned int component_id, const void* buf, int len) {
  if (checkIceState() != IceState::READY) {
    return -1;
  }
  packetPtr packet (new DataPacket());
  memcpy(packet->data, buf, len);
  packet->length = len;
  async([component_id, packet] (std::shared_ptr<LibNiceConnection> this_ptr) {
    int val = -1;
    if (this_ptr->checkIceState() != IceState::READY) {
      return;
    }
    val = this_ptr->lib_nice_->NiceAgentSend(this_ptr->agent_,
                                  this_ptr->stream_id_,
                                  component_id,
                                  packet->length,
                                  reinterpret_cast<const gchar*>(packet->data));
    if (val != packet->length) {
      ELOG_DEBUG("%s message: Sending less data than expected, sent: %d, to_send: %d", this_ptr->toLog(),
                  val,
                  packet->length);
    }
    return;
  });
  return len;
}

void LibNiceConnection::async(std::function<void(std::shared_ptr<LibNiceConnection>)> f) {
  std::weak_ptr<LibNiceConnection> weak_this = shared_from_this();
  io_worker_->task([weak_this, f] {
    if (auto this_ptr = weak_this.lock()) {
      f(this_ptr);
    }
  });
}

void LibNiceConnection::start() {
  async([] (std::shared_ptr<LibNiceConnection> this_ptr) {
    this_ptr->startSync();
  });
}

void LibNiceConnection::startSync() {
    if (this->checkIceState() != INITIAL) {
      return;
    }
    ELOG_DEBUG("%s message: creating Nice Agent", toLog());
    nice_debug_enable(FALSE);

    // Create a nice agent
    agent_ = lib_nice_->NiceAgentNew(io_worker_->getGlibContext(), enable_ice_lite_);

    GValue controlling_mode = { 0 };
    g_value_init(&controlling_mode, G_TYPE_BOOLEAN);

    GValue max_connectivity_checks = { 0 };
    g_value_init(&max_connectivity_checks, G_TYPE_UINT);
    g_value_set_uint(&max_connectivity_checks, 100);
    g_object_set_property(G_OBJECT(agent_), "max-connectivity-checks", &max_connectivity_checks);

    GValue keepalive = { 0 };
    g_value_init(&keepalive, G_TYPE_BOOLEAN);
    g_value_set_boolean(&keepalive, TRUE);
    g_object_set_property(G_OBJECT(agent_), "keepalive-conncheck", &keepalive);

    if (ice_config_.stun_server.compare("") != 0 && ice_config_.stun_port != 0) {
      GValue stun_server = { 0 }, stun_server_port = { 0 };
      g_value_init(&stun_server, G_TYPE_STRING);
      g_value_set_string(&stun_server, ice_config_.stun_server.c_str());
      g_object_set_property(G_OBJECT(agent_), "stun-server", &stun_server);

      g_value_init(&stun_server_port, G_TYPE_UINT);
      g_value_set_uint(&stun_server_port, ice_config_.stun_port);
      g_object_set_property(G_OBJECT(agent_), "stun-server-port", &stun_server_port);

      ELOG_DEBUG("%s message: setting stun, stun_server: %s, stun_port: %d",
                 toLog(), ice_config_.stun_server.c_str(), ice_config_.stun_port);
    }

    // Connect the signals
    g_signal_connect(G_OBJECT(agent_), "candidate-gathering-done",
        G_CALLBACK(LibNiceConnection::candidate_gathering_done), this);
    g_signal_connect(G_OBJECT(agent_), "component-state-changed",
        G_CALLBACK(LibNiceConnection::component_state_change), this);
    g_signal_connect(G_OBJECT(agent_), "new-selected-pair-full",
        G_CALLBACK(LibNiceConnection::new_selected_pair), this);
    g_signal_connect(G_OBJECT(agent_), "new-candidate",
        G_CALLBACK(LibNiceConnection::new_local_candidate), this);

    // Create a new stream and start gathering candidates
    ELOG_DEBUG("%s message: adding stream, iceComponents: %d", toLog(), ice_config_.ice_components);
    stream_id_ = lib_nice_->NiceAgentAddStream(agent_, ice_config_.ice_components);
    gchar *ufrag = NULL, *upass = NULL;
    ELOG_DEBUG("%s stream added, ID %d", toLog(), stream_id_);
    lib_nice_->NiceAgentGetLocalCredentials(agent_, stream_id_, &ufrag, &upass);
    ufrag_ = std::string(ufrag);
    g_free(ufrag);
    upass_ = std::string(upass);
    g_free(upass);
    ELOG_DEBUG("%s local credentials are, ufrag: %s, pass: %s", toLog(), ufrag_.c_str(), upass_.c_str());

    // Set our remote credentials.  This must be done *after* we add a stream.
    if (ice_config_.username.compare("") != 0 && ice_config_.password.compare("") != 0) {
      ELOG_DEBUG("%s message: setting remote credentials in constructor, ufrag:%s, pass:%s",
                 toLog(), ice_config_.username.c_str(), ice_config_.password.c_str());
      this->setRemoteCredentialsSync(ice_config_.username, ice_config_.password);
      g_value_set_boolean(&controlling_mode, FALSE);
    } else {
      ELOG_DEBUG("%s message: No credentials in constructor, setting controlling mode to true", toLog());
      g_value_set_boolean(&controlling_mode, TRUE);
    }

    if (enable_ice_lite_) {
      ELOG_DEBUG("%s message: Enabling Ice Lite, setting controlled mode", toLog());
      g_value_set_boolean(&controlling_mode, FALSE);
    }

    g_object_set_property(G_OBJECT(agent_), "controlling-mode", &controlling_mode);
    // Set Port Range: If this doesn't work when linking the file libnice.sym has to be modified to include this call
    if (ice_config_.min_port != 0 && ice_config_.max_port != 0) {
      ELOG_DEBUG("%s message: setting port range, min_port: %d, max_port: %d",
                 toLog(), ice_config_.min_port, ice_config_.max_port);
      forEachComponent([this] (uint comp_id) {
        lib_nice_->NiceAgentSetPortRange(agent_, (guint)stream_id_, comp_id, (guint)ice_config_.min_port,
            (guint)ice_config_.max_port);
      });
    }

    if (!ice_config_.network_interface.empty()) {
      const char* public_ip = lib_nice_->NiceInterfacesGetIpForInterface(ice_config_.network_interface.c_str());
      if (public_ip) {
        lib_nice_->NiceAgentAddLocalAddress(agent_, public_ip);
      }
    }

    if (ice_config_.turn_server.compare("") != 0 && ice_config_.turn_port != 0) {
      ELOG_DEBUG("%s message: configuring TURN, turn_server: %s , turn_port: %d, turn_username: %s, turn_pass: %s",
                 toLog(), ice_config_.turn_server.c_str(),
          ice_config_.turn_port, ice_config_.turn_username.c_str(), ice_config_.turn_pass.c_str());

      forEachComponent([this] (uint comp_id) {
        lib_nice_->NiceAgentSetRelayInfo(agent_,
            stream_id_,
            comp_id,
            ice_config_.turn_server.c_str(),     // TURN Server IP
            ice_config_.turn_port,               // TURN Server PORT
            ice_config_.turn_username.c_str(),   // Username
            ice_config_.turn_pass.c_str());       // Pass
      });
    }
    if (agent_) {
      forEachComponent([this] (uint comp_id) {
        ELOG_DEBUG("%s message: Attaching recv for comp_id %d", toLog(), comp_id);
        lib_nice_->NiceAgentAttachRecv(agent_, stream_id_, comp_id, io_worker_->getGlibContext(),
          reinterpret_cast<void*>(LibNiceConnection::receive_message),
          this);
      });
    }
    ELOG_DEBUG("%s message: gathering, this: %p", toLog(), this);
    lib_nice_->NiceAgentGatherCandidates(agent_, stream_id_);
}

void LibNiceConnection::maybeRestartIce(std::string remote_ufrag, std::string remote_pass) {
  if (remote_ufrag == ice_config_.username) {
    return;
  }
  async([remote_ufrag, remote_pass]
    (std::shared_ptr<LibNiceConnection> this_ptr) {
      this_ptr->restartIceSync(remote_ufrag, remote_pass);
  });
}

void LibNiceConnection::restartIceSync(std::string remote_ufrag, std::string remote_pass) {
  ELOG_DEBUG("%s message: Starting ice restart", toLog());
  updateIceState(IceState::RESTART);
  bool restart_result = lib_nice_->NiceAgentRestart(agent_);
  ice_config_.username = remote_ufrag;
  ice_config_.password = remote_pass;
  setRemoteCredentialsSync(remote_ufrag, remote_pass);
  gchar *ufrag = NULL, *upass = NULL;
  bool credentials_result = lib_nice_->NiceAgentGetLocalCredentials(agent_, stream_id_, &ufrag, &upass);
  ufrag_ = std::string(ufrag);
  g_free(ufrag);
  upass_ = std::string(upass);
  g_free(upass);
  if (restart_result && credentials_result) {
    // We don't need to gather candidates again but will trigger the event to continue the negotiation
    ELOG_DEBUG("%s message: Ice restart issued successfully", toLog());
    gatheringDone(stream_id_);
  } else {
    // something went wrong with the restart, discard the connection
    forEachComponent([this] (uint comp_id) {
      updateComponentState(comp_id, IceState::FAILED);
    });
  }
}

bool LibNiceConnection::setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool is_bundle) {
  std::vector<CandidateInfo> cands(candidates);
  auto remote_candidates_promise = std::make_shared<std::promise<void>>();
  async([cands, remote_candidates_promise, is_bundle]
          (std::shared_ptr<LibNiceConnection> this_ptr) {
    this_ptr->setRemoteCandidatesSync(cands, is_bundle, remote_candidates_promise);
  });
  std::future<void> remote_candidates_future = remote_candidates_promise->get_future();
  std::future_status status = remote_candidates_future.wait_for(std::chrono::seconds(1));
  if (status == std::future_status::timeout) {
    ELOG_WARN("%s message: Could not set remote candidates", toLog());
    return false;
  }
  return true;
}
bool LibNiceConnection::setRemoteCandidatesSync(const std::vector<CandidateInfo> &cands, bool is_bundle,
    std::shared_ptr<std::promise<void>> remote_candidates_promise) {
  if (agent_ == NULL) {
    close();
    return false;
  }
  GSList* candList = NULL;
  ELOG_DEBUG("%s message: setting remote candidates, candidateSize: %lu, mediaType: %d",
            toLog(), cands.size(), ice_config_.media_type);

  for (unsigned int it = 0; it < cands.size(); it++) {
    NiceCandidateType nice_cand_type;
    CandidateInfo cinfo = cands[it];
    // If bundle we will add the candidates regardless the mediaType
    if (cinfo.componentId != 1 || (!is_bundle && cinfo.mediaType != ice_config_.media_type ))
      continue;

    switch (cinfo.hostType) {
      case HOST:
        nice_cand_type = NICE_CANDIDATE_TYPE_HOST;
        break;
      case SRFLX:
        nice_cand_type = NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE;
        break;
      case PRFLX:
        nice_cand_type = NICE_CANDIDATE_TYPE_PEER_REFLEXIVE;
        break;
      case RELAY:
        nice_cand_type = NICE_CANDIDATE_TYPE_RELAYED;
        break;
      default:
        nice_cand_type = NICE_CANDIDATE_TYPE_HOST;
        break;
    }
    if (cinfo.hostPort == 0) {
      continue;
    }
    NiceCandidate* thecandidate = nice_candidate_new(nice_cand_type);
    if (!remote_ufrag_.empty()) {
      thecandidate->username = strdup(remote_ufrag_.c_str());
      thecandidate->password = strdup(remote_upass_.c_str());
    } else {
      thecandidate->username = NULL;
      thecandidate->password = NULL;
    }
    thecandidate->stream_id = (guint) stream_id_;
    thecandidate->component_id = cinfo.componentId;
    thecandidate->priority = cinfo.priority;
    thecandidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
    nice_address_set_from_string(&thecandidate->addr, cinfo.hostAddress.c_str());
    nice_address_set_port(&thecandidate->addr, cinfo.hostPort);

    std::ostringstream host_info;
    host_info << "hostType: " << cinfo.hostType
        << ", hostAddress: " << cinfo.hostAddress
        << ", hostPort: " << cinfo.hostPort;

    if (cinfo.hostType == RELAY || cinfo.hostType == SRFLX) {
      nice_address_set_from_string(&thecandidate->base_addr, cinfo.rAddress.c_str());
      nice_address_set_port(&thecandidate->base_addr, cinfo.rPort);
      ELOG_DEBUG("%s message: adding relay or srflx remote candidate, %s, rAddress: %s, rPort: %d",
                toLog(), host_info.str().c_str(),
                cinfo.rAddress.c_str(), cinfo.rPort);
    } else {
      ELOG_DEBUG("%s message: adding remote candidate, %s, priority: %d, componentId: %d, ufrag: %s, pass: %s",
          toLog(), host_info.str().c_str(), cinfo.priority, cinfo.componentId, remote_ufrag_.c_str(),
          remote_upass_.c_str());
    }
    candList = g_slist_prepend(candList, thecandidate);
  }
  lib_nice_->NiceAgentSetRemoteCandidates(agent_, (guint)stream_id_, 1, candList);
  g_slist_free_full(candList, (GDestroyNotify)&nice_candidate_free);
  remote_candidates_promise->set_value();
  return true;
}

void LibNiceConnection::gatheringDone(uint stream_id) {
  ELOG_DEBUG("%s message: gathering done, stream_id: %u, stream_id_: %u", toLog(), stream_id, stream_id_);
  updateIceState(IceState::CANDIDATES_RECEIVED);
}

void LibNiceConnection::getCandidate(uint stream_id, uint component_id, const std::string &foundation) {
  GSList* lcands = lib_nice_->NiceAgentGetLocalCandidates(agent_, stream_id, component_id);
  // We only want to get the new candidates
  if (cands_delivered_ <= g_slist_length(lcands)) {
    lcands = g_slist_nth(lcands, (cands_delivered_));
  }
  for (GSList* iterator = lcands; iterator; iterator = iterator->next) {
    char address[NICE_ADDRESS_STRING_LEN], baseAddress[NICE_ADDRESS_STRING_LEN];
    NiceCandidate *cand = reinterpret_cast<NiceCandidate*>(iterator->data);
    nice_address_to_string(&cand->addr, address);
    nice_address_to_string(&cand->base_addr, baseAddress);
    cands_delivered_++;
    if (strstr(address, ":") != NULL) {  // We ignore IPv6 candidates at this point
      continue;
    }
    CandidateInfo cand_info;
    cand_info.componentId = cand->component_id;
    cand_info.foundation = cand->foundation;
    cand_info.priority = cand->priority;
    cand_info.hostAddress = std::string(address);
    cand_info.hostPort = nice_address_get_port(&cand->addr);
    if (cand_info.hostPort == 0) {
      continue;
    }
    cand_info.mediaType = ice_config_.media_type;

    /*
     *    NICE_CANDIDATE_TYPE_HOST,
     *    NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE,
     *    NICE_CANDIDATE_TYPE_PEER_REFLEXIVE,
     *    NICE_CANDIDATE_TYPE_RELAYED,
     */
    switch (cand->type) {
      case NICE_CANDIDATE_TYPE_HOST:
        cand_info.hostType = HOST;
        break;
      case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
        cand_info.hostType = SRFLX;
        cand_info.rAddress = std::string(baseAddress);
        cand_info.rPort = nice_address_get_port(&cand->base_addr);
        break;
      case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
        cand_info.hostType = PRFLX;
        break;
      case NICE_CANDIDATE_TYPE_RELAYED:
        char turnAddres[NICE_ADDRESS_STRING_LEN];
        nice_address_to_string(&cand->turn->server, turnAddres);
        cand_info.hostType = RELAY;
        cand_info.rAddress = std::string(baseAddress);
        cand_info.rPort = nice_address_get_port(&cand->base_addr);
        break;
      default:
        break;
    }
    cand_info.netProtocol = "udp";
    cand_info.transProtocol = ice_config_.transport_name;
    cand_info.username = ufrag_;
    cand_info.password = upass_;
    // localCandidates->push_back(cand_info);
    if (auto listener = this->getIceListener().lock()) {
      ELOG_DEBUG("%s message: Candidate (%s, %s, %s)", toLog(), cand_info.hostAddress.c_str(),
                 ufrag_.c_str(), upass_.c_str());
      listener->onCandidate(cand_info, this);
    }
  }
  // for nice_agent_get_local_candidates, the caller owns the returned GSList as well as the candidates
  // contained within it.
  // let's free everything in the list, as well as the list.
  g_slist_free_full(lcands, (GDestroyNotify)&nice_candidate_free);
}
void LibNiceConnection::setRemoteCredentials(const std::string& username, const std::string& password) {
  auto promise = std::make_shared<std::promise<void>>();
  async([username, password, promise] (std::shared_ptr<LibNiceConnection> this_ptr) {
    this_ptr->setRemoteCredentialsSync(username, password);
    promise->set_value();
  });
  auto status = promise->get_future().wait_for(std::chrono::seconds(1));
  if (status == std::future_status::timeout) {
    ELOG_WARN("%s message: Could not set remote credentials", toLog());
  }
}

void LibNiceConnection::setRemoteCredentialsSync(const std::string& username, const std::string& password) {
  ELOG_DEBUG("%s message: setting remote credentials, ufrag: %s, pass: %s",
             toLog(), username.c_str(), password.c_str());
  remote_ufrag_ = username;
  remote_upass_ = password;
  lib_nice_->NiceAgentSetRemoteCredentials(agent_, (guint) stream_id_, username.c_str(), password.c_str());
}

void LibNiceConnection::updateComponentState(unsigned int component_id, IceState state) {
  ELOG_DEBUG("%s message: new ice component state, newState: %u, transportName: %s, componentId %u, iceComponents: %u",
             toLog(), state, ice_config_.transport_name.c_str(), component_id, ice_config_.ice_components);
  comp_state_list_[component_id] = state;
  if (state == IceState::READY) {
    for (unsigned int i = 1; i <= ice_config_.ice_components; i++) {
      if (comp_state_list_[i] != IceState::READY) {
        return;
      }
    }
  } else if (state == IceState::FAILED) {
    ELOG_WARN("%s message: component failed, ice_config_.transport_name: %s, componentId: %u",
              toLog(), ice_config_.transport_name.c_str(), component_id);
    for (unsigned int i = 1; i <= ice_config_.ice_components; i++) {
      if (comp_state_list_[i] != IceState::FAILED) {
        return;
      }
    }
  }
  this->updateIceState(state);
}


void LibNiceConnection::forEachComponent(std::function<void(uint comp_id)> func) {
  for (unsigned int comp_id = 1; comp_id <= ice_config_.ice_components; comp_id++) {
    func(comp_id);
  }
}

CandidatePair LibNiceConnection::getSelectedPair() {
  char ipaddr[NICE_ADDRESS_STRING_LEN];
  CandidatePair selectedPair;
  NiceCandidate* local, *remote;
  // We only get the selected pair for one component - the most common case
  lib_nice_->NiceAgentGetSelectedPair(agent_, stream_id_, 1, &local, &remote);
  nice_address_to_string(&local->addr, ipaddr);
  selectedPair.erizoCandidateIp = std::string(ipaddr);
  selectedPair.erizoCandidatePort = nice_address_get_port(&local->addr);
  selectedPair.erizoHostType = getHostTypeFromCandidate(local);
  ELOG_DEBUG("%s message: selected pair, local_addr: %s, local_port: %d, local_type: %s",
              toLog(), ipaddr, nice_address_get_port(&local->addr), selectedPair.erizoHostType.c_str());
  nice_address_to_string(&remote->addr, ipaddr);
  selectedPair.clientCandidateIp = std::string(ipaddr);
  selectedPair.clientCandidatePort = nice_address_get_port(&remote->addr);
  selectedPair.clientHostType = getHostTypeFromCandidate(local);
  ELOG_INFO("%s message: selected pair, remote_addr: %s, remote_port: %d, remote_type: %s",
             toLog(), ipaddr, nice_address_get_port(&remote->addr), selectedPair.clientHostType.c_str());
  return selectedPair;
}

std::shared_ptr<IceConnection> LibNiceConnection::create(std::shared_ptr<IOWorker> io_worker,
  const IceConfig& ice_config) {
  auto libnice = std::make_shared<LibNiceConnection>(std::shared_ptr<LibNiceInterface>(new LibNiceInterfaceImpl()),
    io_worker,
    ice_config);

    return libnice;
}
} /* namespace erizo */
