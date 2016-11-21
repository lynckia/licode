/*
 * NiceConnection.cpp
 */

#include <nice/nice.h>
#include <cstdio>
#include <string>
#include <cstring>
#include <vector>

#include "NiceConnection.h"
#include "SdpInfo.h"

using std::memcpy;

// If true (and configured properly below) erizo will generate relay candidates for itself
// MOSTLY USEFUL WHEN ERIZO ITSELF IS BEHIND A NAT
#define SERVER_SIDE_TURN 0

namespace erizo {

DEFINE_LOGGER(NiceConnection, "NiceConnection")


void cb_nice_recv(NiceAgent* agent, guint stream_id, guint component_id,
    guint len, gchar* buf, gpointer user_data) {
  if (user_data == NULL || len == 0) {
    return;
  }
  NiceConnection* nicecon = reinterpret_cast<NiceConnection*>(user_data);
  nicecon->onData(component_id, reinterpret_cast<char*> (buf), static_cast<unsigned int> (len));
}

void cb_new_candidate(NiceAgent *agent, guint stream_id, guint component_id, gchar *foundation,
    gpointer user_data) {
  NiceConnection *conn = reinterpret_cast<NiceConnection*>(user_data);
  std::string found(foundation);
  conn->getCandidate(stream_id, component_id, found);
}

void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer user_data) {
  NiceConnection *conn = reinterpret_cast<NiceConnection*>(user_data);
  conn->gatheringDone(stream_id);
}

void cb_component_state_changed(NiceAgent *agent, guint stream_id,
    guint component_id, guint state, gpointer user_data) {
  if (state == NICE_COMPONENT_STATE_CONNECTED) {
  } else if (state == NICE_COMPONENT_STATE_FAILED) {
    NiceConnection *conn = reinterpret_cast<NiceConnection*>(user_data);
    conn->updateComponentState(component_id, NICE_FAILED);
  }
}

void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
    gchar *lfoundation, gchar *rfoundation, gpointer user_data) {
  NiceConnection *conn = reinterpret_cast<NiceConnection*>(user_data);
  conn->updateComponentState(component_id, NICE_READY);
}

NiceConnection::NiceConnection(boost::shared_ptr<LibNiceInterface> libnice, MediaType med,
    const std::string &transport_name, const std::string& connection_id, NiceConnectionListener* listener,
    unsigned int iceComponents, const IceConfig& iceConfig, std::string username, std::string password) :
  mediaType(med), connection_id_(connection_id), lib_nice_(libnice), agent_(NULL), loop_(NULL), listener_(listener),
  candsDelivered_(0), iceState_(NICE_INITIAL), iceComponents_(iceComponents), username_(username), password_(password),
  iceConfig_(iceConfig), receivedLastCandidate_(false) {
    localCandidates.reset(new std::vector<CandidateInfo>());
    transportName.reset(new std::string(transport_name));
    for (unsigned int i = 1; i <= iceComponents_; i++) {
      comp_state_list_[i] = NICE_INITIAL;
    }
    g_type_init();
  }

NiceConnection::~NiceConnection() {
  ELOG_DEBUG("%s message: destroying", toLog());
  this->close();
  ELOG_DEBUG("%s message: destroyed", toLog());
}

void NiceConnection::close() {
  boost::mutex::scoped_lock(closeMutex_);
  if (this->checkIceState() == NICE_FINISHED) {
    return;
  }
  ELOG_DEBUG("%s message:closing", toLog());
  this->updateIceState(NICE_FINISHED);
  if (loop_ != NULL) {
    ELOG_DEBUG("%s message:main loop quit", toLog());
    g_main_loop_quit(loop_);
  }
  cond_.notify_one();
  listener_ = NULL;
  boost::system_time const timeout = boost::get_system_time() + boost::posix_time::milliseconds(5);
  ELOG_DEBUG("%s message: m_thread join, this: %p", toLog(), this);
  if (!m_Thread_.timed_join(timeout)) {
    ELOG_DEBUG("%s message: interrupt thread to close, this: %p", toLog(), this);
    m_Thread_.interrupt();
  }
  if (loop_ != NULL) {
    ELOG_DEBUG("%s message:Unrefing loop", toLog());
    g_main_loop_unref(loop_);
    loop_ = NULL;
  }
  if (agent_ != NULL) {
    ELOG_DEBUG("%s message: unrefing agent", toLog());
    g_object_unref(agent_);
    agent_ = NULL;
  }
  if (context_ != NULL) {
    ELOG_DEBUG("%s message: Unrefing context", toLog());
    g_main_context_unref(context_);
    context_ = NULL;
  }
  ELOG_DEBUG("%s message: closed, this: %p", toLog(), this);
}

void NiceConnection::onData(unsigned int component_id, char* buf, int len) {
  if (this->checkIceState() == NICE_READY) {
    packetPtr packet (new dataPacket());
    memcpy(packet->data, buf, len);
    packet->comp = component_id;
    packet->length = len;
    listener_->onPacketReceived(packet);
  }
}
int NiceConnection::sendData(unsigned int compId, const void* buf, int len) {
  int val = -1;
  if (this->checkIceState() == NICE_READY) {
    val = lib_nice_->NiceAgentSend(agent_, 1, compId, len, reinterpret_cast<const gchar*>(buf));
  }
  if (val != len) {
    ELOG_DEBUG("%s message: Sending less data than expected, sent: %d, to_send: %d", toLog(), val, len);
  }
  return val;
}

void NiceConnection::start() {
    boost::mutex::scoped_lock(closeMutex_);
    if (this->checkIceState() != NICE_INITIAL) {
      return;
    }
    context_ = g_main_context_new();
    ELOG_DEBUG("%s message: creating Nice Agent", toLog());
    nice_debug_enable(FALSE);
    // Create a nice agent
    agent_ = lib_nice_->NiceAgentNew(context_);
    loop_ = g_main_loop_new(context_, FALSE);
    m_Thread_ = boost::thread(&NiceConnection::mainLoop, this);
    GValue controllingMode = { 0 };
    g_value_init(&controllingMode, G_TYPE_BOOLEAN);
    g_value_set_boolean(&controllingMode, false);
    g_object_set_property(G_OBJECT(agent_), "controlling-mode", &controllingMode);

    GValue checks = { 0 };
    g_value_init(&checks, G_TYPE_UINT);
    g_value_set_uint(&checks, 100);
    g_object_set_property(G_OBJECT(agent_), "max-connectivity-checks", &checks);


    if (iceConfig_.stunServer.compare("") != 0 && iceConfig_.stunPort != 0) {
      GValue val = { 0 }, val2 = { 0 };
      g_value_init(&val, G_TYPE_STRING);
      g_value_set_string(&val, iceConfig_.stunServer.c_str());
      g_object_set_property(G_OBJECT(agent_), "stun-server", &val);

      g_value_init(&val2, G_TYPE_UINT);
      g_value_set_uint(&val2, iceConfig_.stunPort);
      g_object_set_property(G_OBJECT(agent_), "stun-server-port", &val2);

      ELOG_DEBUG("%s message: setting stun, stunServer: %s, stunPort: %d",
                 toLog(), iceConfig_.stunServer.c_str(), iceConfig_.stunPort);
    }

    // Connect the signals
    g_signal_connect(G_OBJECT(agent_), "candidate-gathering-done",
        G_CALLBACK(cb_candidate_gathering_done), this);
    g_signal_connect(G_OBJECT(agent_), "component-state-changed",
        G_CALLBACK(cb_component_state_changed), this);
    g_signal_connect(G_OBJECT(agent_), "new-selected-pair",
        G_CALLBACK(cb_new_selected_pair), this);
    g_signal_connect(G_OBJECT(agent_), "new-candidate",
        G_CALLBACK(cb_new_candidate), this);

    // Create a new stream and start gathering candidates
    ELOG_DEBUG("%s message: adding stream, iceComponents: %d", toLog(), iceComponents_);
    lib_nice_->NiceAgentAddStream(agent_, iceComponents_);
    gchar *ufrag = NULL, *upass = NULL;
    lib_nice_->NiceAgentGetLocalCredentials(agent_, 1, &ufrag, &upass);
    ufrag_ = std::string(ufrag); g_free(ufrag);
    upass_ = std::string(upass); g_free(upass);

    // Set our remote credentials.  This must be done *after* we add a stream.
    if (username_.compare("") != 0 && password_.compare("") != 0) {
      ELOG_DEBUG("%s message: setting remote credentials in constructor, ufrag:%s, pass:%s",
                 toLog(), username_.c_str(), password_.c_str());
      this->setRemoteCredentials(username_, password_);
    }
    // Set Port Range: If this doesn't work when linking the file libnice.sym has to be modified to include this call
    if (iceConfig_.minPort != 0 && iceConfig_.maxPort != 0) {
      ELOG_DEBUG("%s message: setting port range, minPort: %d, maxPort: %d",
                 toLog(), iceConfig_.minPort, iceConfig_.maxPort);
      lib_nice_->NiceAgentSetPortRange(agent_, (guint)1, (guint)1, (guint)iceConfig_.minPort,
          (guint)iceConfig_.maxPort);
    }

    if (iceConfig_.turnServer.compare("") != 0 && iceConfig_.turnPort != 0) {
      ELOG_DEBUG("%s message: configuring TURN, turnServer: %s , turnPort: %d, turnUsername: %s, turnPass: %s",
                 toLog(), iceConfig_.turnServer.c_str(),
          iceConfig_.turnPort, iceConfig_.turnUsername.c_str(), iceConfig_.turnPass.c_str());

      for (unsigned int i = 1; i <= iceComponents_ ; i++) {
        lib_nice_->NiceAgentSetRelayInfo(agent_,
            1,
            i,
            iceConfig_.turnServer.c_str(),     // TURN Server IP
            iceConfig_.turnPort,               // TURN Server PORT
            iceConfig_.turnUsername.c_str(),   // Username
            iceConfig_.turnPass.c_str());       // Pass
      }
    }

    if (agent_) {
      for (unsigned int i = 1; i <= iceComponents_; i++) {
        lib_nice_->NiceAgentAttachRecv(agent_, 1, i, context_, reinterpret_cast<void*>(cb_nice_recv), this);
      }
    }
    ELOG_DEBUG("%s message: gathering, this: %p", toLog(), this);
    lib_nice_->NiceAgentGatherCandidates(agent_, 1);
}

void NiceConnection::mainLoop() {
  // Start gathering candidates and fire event loop
  ELOG_DEBUG("%s message: starting g_main_loop, this: %p", toLog(), this);
  if (agent_ == NULL || loop_ == NULL) {
    return;
  }
  g_main_loop_run(loop_);
  ELOG_DEBUG("%s message: finished g_main_loop, this: %p", toLog(), this);
}

bool NiceConnection::setRemoteCandidates(const std::vector<CandidateInfo> &candidates, bool isBundle) {
  if (agent_ == NULL) {
    this->close();
    return false;
  }
  GSList* candList = NULL;
  ELOG_DEBUG("%s message: setting remote candidates, candidateSize: %lu, mediaType: %d",
             toLog(), candidates.size(), this->mediaType);

  for (unsigned int it = 0; it < candidates.size(); it++) {
    NiceCandidateType nice_cand_type;
    CandidateInfo cinfo = candidates[it];
    // If bundle we will add the candidates regardless the mediaType
    if (cinfo.componentId != 1 || (!isBundle && cinfo.mediaType != this->mediaType ))
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
    NiceCandidate* thecandidate = nice_candidate_new(nice_cand_type);
    thecandidate->username = strdup(cinfo.username.c_str());
    thecandidate->password = strdup(cinfo.password.c_str());
    thecandidate->stream_id = (guint) 1;
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
          toLog(), host_info.str().c_str(), cinfo.priority, cinfo.componentId, cinfo.username.c_str(),
          cinfo.password.c_str());
    }
    candList = g_slist_prepend(candList, thecandidate);
  }
  // TODO(pedro): Set Component Id properly, now fixed at 1
  lib_nice_->NiceAgentSetRemoteCandidates(agent_, (guint) 1, 1, candList);
  g_slist_free_full(candList, (GDestroyNotify)&nice_candidate_free);

  return true;
}

void NiceConnection::gatheringDone(uint stream_id) {
  ELOG_DEBUG("%s message: gathering done, stream_id: %u", toLog(), stream_id);
  this->updateIceState(NICE_CANDIDATES_RECEIVED);
}

void NiceConnection::getCandidate(uint stream_id, uint component_id, const std::string &foundation) {
  GSList* lcands = lib_nice_->NiceAgentGetLocalCandidates(agent_, stream_id, component_id);
  // We only want to get the new candidates
  if (candsDelivered_ <= g_slist_length(lcands)) {
    lcands = g_slist_nth(lcands, (candsDelivered_));
  }
  for (GSList* iterator = lcands; iterator; iterator = iterator->next) {
    char address[NICE_ADDRESS_STRING_LEN], baseAddress[NICE_ADDRESS_STRING_LEN];
    NiceCandidate *cand = reinterpret_cast<NiceCandidate*>(iterator->data);
    nice_address_to_string(&cand->addr, address);
    nice_address_to_string(&cand->base_addr, baseAddress);
    candsDelivered_++;
    if (strstr(address, ":") != NULL) {  // We ignore IPv6 candidates at this point
      continue;
    }
    CandidateInfo cand_info;
    cand_info.componentId = cand->component_id;
    cand_info.foundation = cand->foundation;
    cand_info.priority = cand->priority;
    cand_info.hostAddress = std::string(address);
    cand_info.hostPort = nice_address_get_port(&cand->addr);
    cand_info.mediaType = mediaType;

    /*
     *   NICE_CANDIDATE_TYPE_HOST,
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
    cand_info.transProtocol = std::string(*transportName.get());
    cand_info.username = ufrag_;
    cand_info.password = upass_;
    // localCandidates->push_back(cand_info);
    if (this->getNiceListener() != NULL)
      this->getNiceListener()->onCandidate(cand_info, this);
  }
  // for nice_agent_get_local_candidates, the caller owns the returned GSList as well as the candidates
  // contained within it.
  // let's free everything in the list, as well as the list.
  g_slist_free_full(lcands, (GDestroyNotify)&nice_candidate_free);
}

void NiceConnection::setRemoteCredentials(const std::string& username, const std::string& password) {
  ELOG_DEBUG("%s message: setting remote credentials, ufrag: %s, pass: %s",
             toLog(), username.c_str(), password.c_str());
  lib_nice_->NiceAgentSetRemoteCredentials(agent_, (guint) 1, username.c_str(), password.c_str());
}

void NiceConnection::setNiceListener(NiceConnectionListener *listener) {
  this->listener_ = listener;
}

NiceConnectionListener* NiceConnection::getNiceListener() {
  return this->listener_;
}

void NiceConnection::updateComponentState(unsigned int compId, IceState state) {
  ELOG_DEBUG("%s message: new ice component state, newState: %u, transportName: %s, componentId %u, iceComponents: %u",
             toLog(), state, transportName->c_str(), compId, iceComponents_);
  comp_state_list_[compId] = state;
  if (state == NICE_READY) {
    for (unsigned int i = 1; i <= iceComponents_; i++) {
      if (comp_state_list_[i] != NICE_READY) {
        return;
      }
    }
  } else if (state == NICE_FAILED) {
    if (receivedLastCandidate_) {
      ELOG_WARN("%s message: component failed, transportName: %s, componentId: %u",
                toLog(), transportName->c_str(), compId);
      for (unsigned int i = 1; i <= iceComponents_; i++) {
        if (comp_state_list_[i] != NICE_FAILED) {
          return;
        }
      }
    } else {
      ELOG_WARN("%s message: failed and not received all candidates, newComponentState:%u", toLog(), state);
      return;
    }
  }
  this->updateIceState(state);
}

IceState NiceConnection::checkIceState() {
  return iceState_;
}

std::string NiceConnection::iceStateToString(IceState state) const {
  switch (state) {
    case NICE_INITIAL:             return "initial";
    case NICE_FINISHED:            return "finished";
    case NICE_FAILED:              return "failed";
    case NICE_READY:               return "ready";
    case NICE_CANDIDATES_RECEIVED: return "cand_received";
  }
  return "unknown";
}

void NiceConnection::updateIceState(IceState state) {
  if (state <= iceState_) {
    if (state != NICE_READY)
      ELOG_WARN("%s message: unexpected ice state transition, iceState: %s,  newIceState: %s",
                 toLog(), iceStateToString(iceState_).c_str(), iceStateToString(state).c_str());
    return;
  }

  ELOG_INFO("%s message: iceState transition, transportName: %s, iceState: %s, newIceState: %s, this: %p",
             toLog(), transportName->c_str(),
             iceStateToString(this->iceState_).c_str(), iceStateToString(state).c_str(), this);
  this->iceState_ = state;
  switch (iceState_) {
    case NICE_FINISHED:
      return;
    case NICE_FAILED:
      ELOG_WARN("%s message: Ice Failed", toLog());
      break;

    case NICE_READY:
    case NICE_CANDIDATES_RECEIVED:
      break;
    default:
      break;
  }

  // Important: send this outside our state lock.  Otherwise, serious risk of deadlock.
  if (this->listener_ != NULL)
    this->listener_->updateIceState(state, this);
}

CandidatePair NiceConnection::getSelectedPair() {
  char ipaddr[NICE_ADDRESS_STRING_LEN];
  CandidatePair selectedPair;
  NiceCandidate* local, *remote;
  lib_nice_->NiceAgentGetSelectedPair(agent_, 1, 1, &local, &remote);
  nice_address_to_string(&local->addr, ipaddr);
  selectedPair.erizoCandidateIp = std::string(ipaddr);
  selectedPair.erizoCandidatePort = nice_address_get_port(&local->addr);
  ELOG_DEBUG("%s message: selected pair, local_addr: %s, local_port: %d",
              toLog(), ipaddr, nice_address_get_port(&local->addr));
  nice_address_to_string(&remote->addr, ipaddr);
  selectedPair.clientCandidateIp = std::string(ipaddr);
  selectedPair.clientCandidatePort = nice_address_get_port(&remote->addr);
  ELOG_INFO("%s message: selected pair, remote_addr: %s, remote_port: %d",
             toLog(), ipaddr, nice_address_get_port(&remote->addr));
  return selectedPair;
}

void NiceConnection::setReceivedLastCandidate(bool hasReceived) {
  ELOG_DEBUG("%s message: setting hasReceivedLastCandidate, hasReceived: %u", toLog(), hasReceived);
  this->receivedLastCandidate_ = hasReceived;
}
} /* namespace erizo */
