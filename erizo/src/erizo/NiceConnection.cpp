/*
 * NiceConnection.cpp
 */

#include <nice/nice.h>
#include <cstdio>
#include <poll.h>

#include "NiceConnection.h"
#include "SdpInfo.h"


// If true (and configured properly below) erizo will generate relay candidates for itself MOSTLY USEFUL WHEN ERIZO ITSELF IS BEHIND A NAT
#define SERVER_SIDE_TURN 0

namespace erizo {
  
  DEFINE_LOGGER(NiceConnection, "NiceConnection")


  struct queue_not_empty
  {
    std::queue<packetPtr>& queue;

    queue_not_empty(std::queue<packetPtr>& queue_):
      queue(queue_)
    {}
    bool operator()() const
    {
      return !queue.empty();
    }
  };


  int timed_poll(GPollFD* fds, guint nfds, gint timeout){
    return poll((pollfd*)fds,nfds,200);
  }

  void cb_nice_recv(NiceAgent* agent, guint stream_id, guint component_id,
      guint len, gchar* buf, gpointer user_data) {
    if (user_data==NULL||len==0)return;
    NiceConnection* nicecon = (NiceConnection*) user_data;
    nicecon->queueData(component_id, reinterpret_cast<char*> (buf), static_cast<unsigned int> (len));
  }

  void cb_new_candidate(NiceAgent *agent, guint stream_id, guint component_id, gchar *foundation,
      gpointer user_data) {
    NiceConnection *conn = (NiceConnection*) user_data;
    std::string found(foundation);
    conn->getCandidate(stream_id, component_id, found);
  }

  void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
      gpointer user_data) {

    NiceConnection *conn = (NiceConnection*) user_data;
    conn->gatheringDone(stream_id);
  }

  void cb_component_state_changed(NiceAgent *agent, guint stream_id,
      guint component_id, guint state, gpointer user_data) {
    if (state == NICE_COMPONENT_STATE_CONNECTED) {
//      NiceConnection *conn = (NiceConnection*) user_data;
//      conn->updateComponentState(component_id, NICE_READY);
    } else if (state == NICE_COMPONENT_STATE_FAILED) {
      NiceConnection *conn = (NiceConnection*) user_data;
      conn->updateComponentState(component_id, NICE_FAILED);
    }

  }

  void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
      gchar *lfoundation, gchar *rfoundation, gpointer user_data) {
    NiceConnection *conn = (NiceConnection*) user_data;
    conn->updateComponentState(component_id, NICE_READY);
  }

  NiceConnection::NiceConnection(MediaType med, const std::string &transport_name,NiceConnectionListener* listener, 
      unsigned int iceComponents, const std::string& stunServer, int stunPort, int minPort, int maxPort, std::string username, std::string password)
     : mediaType(med), agent_(NULL), listener_(listener), candsDelivered_(0), context_(NULL), iceState_(NICE_INITIAL), iceComponents_(iceComponents) {

    localCandidates.reset(new std::vector<CandidateInfo>());
    transportName.reset(new std::string(transport_name));
    for (unsigned int i = 1; i<=iceComponents_; i++) {
      comp_state_list_[i] = NICE_INITIAL;
    }
    
    g_type_init();
    context_ = g_main_context_new();
    g_main_context_set_poll_func(context_,timed_poll);
    ELOG_DEBUG("Creating Agent");
    nice_debug_enable( FALSE );
    // Create a nice agent
    agent_ = nice_agent_new(context_, NICE_COMPATIBILITY_RFC5245);
    GValue controllingMode = { 0 };
    g_value_init(&controllingMode, G_TYPE_BOOLEAN);
    g_value_set_boolean(&controllingMode, false);
    g_object_set_property(G_OBJECT( agent_ ), "controlling-mode", &controllingMode);

    GValue checks = { 0 };
    g_value_init(&checks, G_TYPE_UINT);
    g_value_set_uint(&checks, 100);
    g_object_set_property(G_OBJECT( agent_ ), "max-connectivity-checks", &checks);


    if (stunServer.compare("") != 0 && stunPort!=0){
      GValue val = { 0 }, val2 = { 0 };
      g_value_init(&val, G_TYPE_STRING);
      g_value_set_string(&val, stunServer.c_str());
      g_object_set_property(G_OBJECT( agent_ ), "stun-server", &val);

      g_value_init(&val2, G_TYPE_UINT);
      g_value_set_uint(&val2, stunPort);
      g_object_set_property(G_OBJECT( agent_ ), "stun-server-port", &val2);

      ELOG_DEBUG("Setting STUN server %s:%d", stunServer.c_str(), stunPort);
    }

    // Connect the signals
    g_signal_connect( G_OBJECT( agent_ ), "candidate-gathering-done",
        G_CALLBACK( cb_candidate_gathering_done ), this);
    g_signal_connect( G_OBJECT( agent_ ), "component-state-changed",
        G_CALLBACK( cb_component_state_changed ), this);
    g_signal_connect( G_OBJECT( agent_ ), "new-selected-pair",
        G_CALLBACK( cb_new_selected_pair ), this);
    g_signal_connect( G_OBJECT( agent_ ), "new-candidate",
        G_CALLBACK( cb_new_candidate ), this);

    // Create a new stream and start gathering candidates
    ELOG_DEBUG("Adding Stream... Number of components %d", iceComponents_);
    nice_agent_add_stream(agent_, iceComponents_);
    gchar *ufrag = NULL, *upass = NULL;
    nice_agent_get_local_credentials(agent_, 1, &ufrag, &upass);
    ufrag_ = std::string(ufrag); g_free(ufrag);
    upass_ = std::string(upass); g_free(upass);

    // Set our remote credentials.  This must be done *after* we add a stream.
    nice_agent_set_remote_credentials(agent_, (guint) 1, username.c_str(), password.c_str());

    // Set Port Range ----> If this doesn't work when linking the file libnice.sym has to be modified to include this call
    if (minPort!=0 && maxPort!=0){
      ELOG_DEBUG("Setting port range: %d to %d\n", minPort, maxPort);
      nice_agent_set_port_range(agent_, (guint)1, (guint)1, (guint)minPort, (guint)maxPort);
    }

    if (SERVER_SIDE_TURN){
        for (unsigned int i = 1; i <= iceComponents_ ; i++){
          ELOG_DEBUG("Setting TURN Comp %d\n", i);
          nice_agent_set_relay_info     (agent_,
              1,
              i,
              "",      // TURN Server IP
              3479,    // TURN Server PORT
              "",      // Username
              "",      // Pass
              NICE_RELAY_TYPE_TURN_UDP);
        }
    }
    ELOG_DEBUG("Gathering candidates %p", this);
    nice_agent_gather_candidates(agent_, 1);
    if(agent_){
      for (unsigned int i = 1; i<=iceComponents_; i++){
        nice_agent_attach_recv(agent_, 1, i, context_, cb_nice_recv, this);
      }
      running_ = true;
    }
    else{
      running_=false;
    }
  m_Thread_ = boost::thread(&NiceConnection::init, this);
}

  NiceConnection::~NiceConnection() {
    ELOG_DEBUG("NiceConnection Destructor");
    this->close();
    ELOG_DEBUG("NiceConnection Destructor END");
  }
  
  packetPtr NiceConnection::getPacket(){
      if(this->checkIceState()==NICE_FINISHED || !running_) {
          packetPtr p (new dataPacket());
          p->length=-1;
          return p;
      }
      boost::unique_lock<boost::mutex> lock(queueMutex_);
      boost::system_time const timeout=boost::get_system_time()+ boost::posix_time::milliseconds(300);
      if(!cond_.timed_wait(lock,timeout, queue_not_empty(niceQueue_))){
        packetPtr p (new dataPacket());
        p->length=0;
        return p;
      }

      if(this->checkIceState()==NICE_FINISHED || !running_) {
          packetPtr p (new dataPacket());
          p->length=-1;
          return p;
      }
      if(!niceQueue_.empty()){
        packetPtr p (niceQueue_.front());
        niceQueue_.pop();
        return  p;
      }
      packetPtr p (new dataPacket());
      p->length=0;
      return p;
  }

  void NiceConnection::close() {
    if(this->checkIceState()==NICE_FINISHED){
      return;
    }
    running_ = false;
    ELOG_DEBUG("Closing nice  %p", this);
    this->updateIceState(NICE_FINISHED);
    cond_.notify_one();
    listener_ = NULL;
    boost::system_time const timeout=boost::get_system_time()+ boost::posix_time::milliseconds(500);
    ELOG_DEBUG("m_thread join %p", this);
    if (!m_Thread_.timed_join(timeout) ){
      ELOG_DEBUG("Taking too long to close thread, trying to interrupt %p", this);
      m_Thread_.interrupt();
    }

    if (agent_!=NULL){
      g_object_unref(agent_);
      agent_ = NULL;
    }
    if (context_!=NULL) {
      g_main_context_unref(context_);
      context_=NULL;
    }

    ELOG_DEBUG("Nice Closed %p", this);
  }

  void NiceConnection::queueData(unsigned int component_id, char* buf, int len){
    if (this->checkIceState() == NICE_READY && running_){
      boost::mutex::scoped_lock(queueMutex_);
      if (niceQueue_.size() < 1000 ) {
        packetPtr p_ (new dataPacket());
        memcpy(p_->data, buf, len);
        p_->comp = component_id;
        p_->length = len;
        niceQueue_.push(p_);
        cond_.notify_one();
      }
    }
  
  }
  int NiceConnection::sendData(unsigned int compId, const void* buf, int len) {
    int val = -1;
    if (this->checkIceState() == NICE_READY && running_) {
      val = nice_agent_send(agent_, 1, compId, len, reinterpret_cast<const gchar*>(buf));
    }
    if (val != len) {
      ELOG_DEBUG("Data sent %d of %d", val, len);
    }
    return val;
  }

  void NiceConnection::init() {

    // Attach to the component to receive the data
    while(running_){
      if(this->checkIceState()>=NICE_FINISHED || !running_)
        break;
      g_main_context_iteration(context_, true);
    }
    ELOG_DEBUG("LibNice thread finished %p", this);
  }

  bool NiceConnection::setRemoteCandidates(std::vector<CandidateInfo> &candidates, bool isBundle) {
    if(agent_==NULL){
      running_=false;
      return false;
    }
    GSList* candList = NULL;
    ELOG_DEBUG("Setting remote candidates %lu, mediatype %d", candidates.size(), this->mediaType);

    for (unsigned int it = 0; it < candidates.size(); it++) {
      NiceCandidateType nice_cand_type;
      CandidateInfo cinfo = candidates[it];
      //If bundle we will add the candidates regardless the mediaType
      if (cinfo.componentId !=1 || (!isBundle && cinfo.mediaType!=this->mediaType ))
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
      
      if (cinfo.hostType == RELAY||cinfo.hostType==SRFLX){
        nice_address_set_from_string(&thecandidate->base_addr, cinfo.rAddress.c_str());
        nice_address_set_port(&thecandidate->base_addr, cinfo.rPort);
        ELOG_DEBUG("Adding remote candidate type %d addr %s port %d raddr %s rport %d", cinfo.hostType, cinfo.hostAddress.c_str(), cinfo.hostPort,
            cinfo.rAddress.c_str(), cinfo.rPort);
      }else{
        ELOG_DEBUG("Adding remote candidate type %d addr %s port %d priority %d componentId %d, username %s, pass %s", 
          cinfo.hostType, 
          cinfo.hostAddress.c_str(), 
          cinfo.hostPort, 
          cinfo.priority, 
          cinfo.componentId,
          cinfo.username.c_str(),
          cinfo.password.c_str()
          );
      }
      candList = g_slist_prepend(candList, thecandidate);
    }
    //TODO: Set Component Id properly, now fixed at 1 
    nice_agent_set_remote_candidates(agent_, (guint) 1, 1, candList);
    g_slist_free_full(candList, (GDestroyNotify)&nice_candidate_free);
    
    return true;
  }

  void NiceConnection::gatheringDone(uint stream_id){
    ELOG_DEBUG("Gathering done for stream %u", stream_id);
  }

  void NiceConnection::getCandidate(uint stream_id, uint component_id, const std::string &foundation) {
    GSList* lcands = nice_agent_get_local_candidates(agent_, stream_id, component_id);
    // We only want to get the new candidates
    int listLength = g_slist_length(lcands);
    ELOG_DEBUG("List length %u, candsDelivered %u", listLength, candsDelivered_);
    if (candsDelivered_ <= g_slist_length(lcands)){
      lcands = g_slist_nth(lcands, (candsDelivered_));
    }
    ELOG_DEBUG("getCandidate %u", g_slist_length(lcands));
    for (GSList* iterator = lcands; iterator; iterator = iterator->next) {
      ELOG_DEBUG("Candidate");
      char address[NICE_ADDRESS_STRING_LEN], baseAddress[NICE_ADDRESS_STRING_LEN];
      NiceCandidate *cand = (NiceCandidate*) iterator->data;
      nice_address_to_string(&cand->addr, address);
      nice_address_to_string(&cand->base_addr, baseAddress);
      candsDelivered_++;
      if (strstr(address, ":") != NULL) { // We ignore IPv6 candidates at this point
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
          ELOG_DEBUG("TURN LOCAL CANDIDATE");
          nice_address_to_string(&cand->turn->server,turnAddres);
          ELOG_DEBUG("address %s", address);
          ELOG_DEBUG("baseAddress %s", baseAddress);
          ELOG_DEBUG("stream_id %u", cand->stream_id);
          ELOG_DEBUG("priority %u", cand->priority);
          ELOG_DEBUG("TURN ADDRESS %s", turnAddres);

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
      //localCandidates->push_back(cand_info);
      this->getNiceListener()->onCandidate(cand_info, this);
    }
    // for nice_agent_get_local_candidates,  the caller owns the returned GSList as well as the candidates contained within it.
    // let's free everything in the list, as well as the list.
    g_slist_free_full(lcands, (GDestroyNotify)&nice_candidate_free);

  }

  void NiceConnection::getLocalCredentials(std::string& username, std::string& password) {
    username = ufrag_;
    password = upass_;
  }

  void NiceConnection::setNiceListener(NiceConnectionListener *listener) {
    this->listener_ = listener;
  }

  NiceConnectionListener* NiceConnection::getNiceListener() {
    return this->listener_;
  }

  void NiceConnection::updateComponentState(unsigned int compId, IceState state) {
    ELOG_DEBUG("%s - NICE Component State Changed %u - %u", transportName->c_str(), compId, state);
    comp_state_list_[compId] = state;
    if (state == NICE_READY) {
      for (unsigned int i = 1; i<=iceComponents_; i++) {
        if (comp_state_list_[i] != NICE_READY) {
          return;
        }
      }
    }else if (state == NICE_FAILED){
      ELOG_ERROR("%s - NICE Component %u FAILED", transportName->c_str(), compId);
      for (unsigned int i = 1; i<=iceComponents_; i++) {
        if (comp_state_list_[i] != NICE_FAILED) {
          return;
        }
      }
    }
    this->updateIceState(state);
  }

  IceState NiceConnection::checkIceState(){
    return iceState_;
  }

  void NiceConnection::updateIceState(IceState state) {

    if(iceState_==state)
      return;

    ELOG_INFO("%s - NICE State Changing from %u to %u %p", transportName->c_str(), this->iceState_, state, this);
    this->iceState_ = state;
    switch( iceState_) {
      case NICE_FINISHED:
        return;
      case NICE_FAILED:
        ELOG_ERROR("Nice Failed, stopping ICE");
        this->running_=false;
        break;

      case NICE_READY:
        break;
      default:
        break;
    }

    // Important: send this outside our state lock.  Otherwise, serious risk of deadlock.
    if (this->listener_ != NULL)
      this->listener_->updateIceState(state, this);
  }

  CandidatePair NiceConnection::getSelectedPair(){
    char ipaddr[NICE_ADDRESS_STRING_LEN];
    CandidatePair selectedPair;
    NiceCandidate* local, *remote;
    nice_agent_get_selected_pair(agent_, 1, 1, &local, &remote);
    nice_address_to_string(&local->addr, ipaddr);
    selectedPair.erizoCandidateIp = std::string(ipaddr);
    selectedPair.erizoCandidatePort = nice_address_get_port(&local->addr);
    ELOG_INFO("Selected pair:\nlocal candidate addr: %s:%d",ipaddr, nice_address_get_port(&local->addr));
    nice_address_to_string(&remote->addr, ipaddr);
    selectedPair.clientCandidateIp = std::string(ipaddr);
    selectedPair.clientCandidatePort = nice_address_get_port(&remote->addr);
    ELOG_INFO("remote candidate addr: %s:%d",ipaddr, nice_address_get_port(&remote->addr));
    return selectedPair;

  }

} /* namespace erizo */
