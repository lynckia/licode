/*
 * NiceConnection.cpp
 */

#include <glib.h>
#include <nice/nice.h>
#include <cstdio>
#include <poll.h>

#include "NiceConnection.h"
#include "SdpInfo.h"


// If true (and configured properly below) erizo will generate relay candidates for itself MOSTLY USEFUL WHEN ERIZO ITSELF IS BEHIND A NAT
#define SERVER_SIDE_TURN 0

namespace erizo {
  
  DEFINE_LOGGER(NiceConnection, "NiceConnection");
  guint stream_id;
  GSList* lcands;
  int streamsGathered;
  int rec, sen;
  int length;

  int timed_poll(GPollFD* fds, guint nfds, gint timeout){
    return poll((pollfd*)fds,nfds,200);
  }
  void cb_nice_recv(NiceAgent* agent, guint stream_id, guint component_id,
      guint len, gchar* buf, gpointer user_data) {
    if (user_data==NULL||len==0)return;

    NiceConnection* nicecon = (NiceConnection*) user_data;
    nicecon->queueData(component_id, reinterpret_cast<char*> (buf), static_cast<unsigned int> (len));
  }

  void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
      gpointer user_data) {

    NiceConnection *conn = (NiceConnection*) user_data;
    conn->gatheringDone(stream_id);
  }

  void cb_component_state_changed(NiceAgent *agent, guint stream_id,
      guint component_id, guint state, gpointer user_data) {
    if (state == NICE_COMPONENT_STATE_READY) {
      NiceConnection *conn = (NiceConnection*) user_data;
      conn->updateComponentState(component_id, NICE_READY);
    } else if (state == NICE_COMPONENT_STATE_FAILED) {
      NiceConnection *conn = (NiceConnection*) user_data;
      conn->updateIceState(NICE_FAILED);
    }

  }

  void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
      gchar *lfoundation, gchar *rfoundation, gpointer user_data) {
    NiceConnection *conn = (NiceConnection*) user_data;
    conn->updateComponentState(component_id, NICE_READY);
  }

  NiceConnection::NiceConnection(MediaType med, const std::string &transport_name,NiceConnectionListener* listener, unsigned int iceComponents, const std::string& stunServer,
                                  int stunPort, int minPort, int maxPort)
     : mediaType(med), agent_(NULL), listener_(listener), loop_(NULL), context_(NULL), iceState_(NICE_INITIAL), iceComponents_(iceComponents),
             stunServer_(stunServer), stunPort_ (stunPort), minPort_(minPort), maxPort_(maxPort) {
    localCandidates.reset(new std::vector<CandidateInfo>());
    transportName.reset(new std::string(transport_name));
    for (unsigned int i = 1; i<=iceComponents; i++) {
      comp_state_list_[i] = NICE_INITIAL;
    }
    running_ = true;
    m_Thread_ = boost::thread(&NiceConnection::init, this);
  }

  NiceConnection::~NiceConnection() {
    ELOG_DEBUG("NiceConnection Destructor");
    this->close();
    boost::mutex::scoped_lock lock(queueMutex_);
    ELOG_DEBUG("NiceConnection Destructor END");
  }
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
  packetPtr NiceConnection::getPacket(){
      boost::unique_lock<boost::mutex> lock(queueMutex_);
      boost::system_time const timeout=boost::get_system_time()+ boost::posix_time::milliseconds(300);
      if(!cond_.timed_wait(lock,timeout, queue_not_empty(niceQueue_))){
        packetPtr p (new dataPacket());
        p->length=0;
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
    boost::unique_lock<boost::mutex> lock(agentMutex_);
    this->updateIceState(NICE_FINISHED);
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
    lock.unlock();
    this->queueData(1, NULL, -1 );
    ELOG_DEBUG("Nice Closed %p", this);
  }

  void NiceConnection::start() {
  }

  void NiceConnection::queueData(unsigned int component_id, char* buf, int len){
    boost::mutex::scoped_lock(queueMutex_);
    if (this->checkIceState() == NICE_READY){
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
    boost::mutex::scoped_lock(agentMutex_);
    if (this->checkIceState() == NICE_READY) {
      val = nice_agent_send(agent_, 1, compId, len, reinterpret_cast<const gchar*>(buf));
    }
    if (val != len) {
      ELOG_DEBUG("Data sent %d of %d", val, len);
    }
    return val;
  }

  void NiceConnection::init() {
    if(this->checkIceState() != NICE_INITIAL){
      ELOG_DEBUG("Initing NiceConnection not in INITIAL state, exiting... %p", this);
      return;
    };
    boost::unique_lock<boost::mutex> lock(agentMutex_);
    if(!running_)
      return;
    streamsGathered = 0;
    this->updateIceState(NICE_INITIAL);

    g_type_init();
    context_ = g_main_context_new();
    g_main_context_set_poll_func(context_,timed_poll);
    /* loop_ =  g_main_loop_new(context_, FALSE); */
    ELOG_DEBUG("Creating Agent");
    //loop_ =  g_main_loop_new(NULL, FALSE);
    //	nice_debug_enable( TRUE );
    // Create a nice agent
    //agent_ = nice_agent_new(g_main_loop_get_context(loop_), NICE_COMPATIBILITY_RFC5245);
    agent_ = nice_agent_new(context_, NICE_COMPATIBILITY_RFC5245);
    GValue controllingMode = { 0 };
    g_value_init(&controllingMode, G_TYPE_BOOLEAN);
    g_value_set_boolean(&controllingMode, false);
    g_object_set_property(G_OBJECT( agent_ ), "controlling-mode", &controllingMode);

    //	NiceAddress* naddr = nice_address_new();
    //	nice_agent_add_local_address(agent_, naddr);

    if (stunServer_.compare("") != 0 && stunPort_!=0){
      GValue val = { 0 }, val2 = { 0 };
      g_value_init(&val, G_TYPE_STRING);
      g_value_set_string(&val, stunServer_.c_str());
      g_object_set_property(G_OBJECT( agent_ ), "stun-server", &val);

      g_value_init(&val2, G_TYPE_UINT);
      g_value_set_uint(&val2, stunPort_);
      g_object_set_property(G_OBJECT( agent_ ), "stun-server-port", &val2);

      ELOG_DEBUG("Setting STUN server %s:%d", stunServer_.c_str(), stunPort_);
    }

    // Connect the signals
    g_signal_connect( G_OBJECT( agent_ ), "candidate-gathering-done",
        G_CALLBACK( cb_candidate_gathering_done ), this);
    g_signal_connect( G_OBJECT( agent_ ), "component-state-changed",
        G_CALLBACK( cb_component_state_changed ), this);
    g_signal_connect( G_OBJECT( agent_ ), "new-selected-pair",
        G_CALLBACK( cb_new_selected_pair ), this);

    // Create a new stream and start gathering candidates
    ELOG_DEBUG("Adding Stream... Number of components %d", iceComponents_);
    nice_agent_add_stream(agent_, iceComponents_);
    
    // Set Port Range ----> If this doesn't work when linking the file libnice.sym has to be modified to include this call
   
    if (minPort_!=0 && maxPort_!=0){
      ELOG_DEBUG("Setting port range: %d to %d\n", minPort_, maxPort_);
      nice_agent_set_port_range(agent_, (guint)1, (guint)1, (guint)minPort_, (guint)maxPort_);
    }
    
    if (SERVER_SIDE_TURN){
        for (int i = 1; i < (iceComponents_ +1); i++){
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
      nice_agent_attach_recv(agent_, 1, 1, context_,
          cb_nice_recv, this);
      if (iceComponents_ > 1) {
        nice_agent_attach_recv(agent_, 1, 2, context_,
            cb_nice_recv, this);
      }
    }else{
      running_=false;
    }
    // Attach to the component to receive the data
    lock.unlock();
    while(running_){
      boost::unique_lock<boost::mutex> lockContext(agentMutex_);
      if(this->checkIceState()>=NICE_FINISHED)
        break;
      g_main_context_iteration(context_, true);
      lockContext.unlock();
    }
    ELOG_DEBUG("LibNice thread finished %p", this);
  }

  bool NiceConnection::setRemoteCandidates(
      std::vector<CandidateInfo> &candidates) {
    if(agent_==NULL){
      running_=false;
      return false;
    }

    ELOG_DEBUG("Setting remote candidates %lu", candidates.size());
    for (unsigned int compId = 1; compId <= iceComponents_; compId++) {

      GSList* candList = NULL;

      for (unsigned int it = 0; it < candidates.size(); it++) {
        NiceCandidateType nice_cand_type;
        CandidateInfo cinfo = candidates[it];
        if (cinfo.mediaType != this->mediaType
            || cinfo.componentId != compId)
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
        sprintf(thecandidate->foundation, "%s", cinfo.foundation.c_str());
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
          ELOG_DEBUG("Adding remote candidate type %d addr %s port %d", cinfo.hostType, cinfo.hostAddress.c_str(), cinfo.hostPort);
        }
        candList = g_slist_prepend(candList, thecandidate);
      }

      nice_agent_set_remote_candidates(agent_, (guint) 1, compId, candList);
      g_slist_free_full(candList, (GDestroyNotify)&nice_candidate_free);
    }

    ELOG_DEBUG("Finished setting candidates\n");
    this->updateIceState(NICE_CANDIDATES_RECEIVED);
    return true;
  }


  void NiceConnection::gatheringDone(uint stream_id) {
    if (this->checkIceState() >= NICE_FINISHED) {
      ELOG_DEBUG("gathering Done after FINISHED");
      nice_agent_remove_stream (agent_,1);
      return;
    }
    ELOG_DEBUG("Gathering Done %p", this);
    int currentCompId = 1;
    lcands = nice_agent_get_local_candidates(agent_, stream_id, currentCompId++);
    NiceCandidate *cand;
    GSList* iterator;
    gchar *ufrag = NULL, *upass = NULL;
    nice_agent_get_local_credentials(agent_, stream_id, &ufrag, &upass);
    //////False candidate for testing when there is no network (like in the train) :)
    /*
       NiceCandidate* thecandidate = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
       NiceAddress* naddr = nice_address_new();
       nice_address_set_from_string(naddr, "127.0.0.1");
       nice_address_set_port(naddr, 50000);
       thecandidate->addr = *naddr;
       char* uname = (char*) malloc(50);
       char* pass = (char*) malloc(50);
       sprintf(thecandidate->foundation, "%s", "1");
       sprintf(uname, "%s", "Pedro");
       sprintf(pass, "%s", "oooo");

       thecandidate->username = uname;
       thecandidate->password = pass;
       thecandidate->stream_id = (guint) 1;
       thecandidate->component_id = 1;
       thecandidate->priority = 1000;
       thecandidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
       lcands = g_slist_append(lcands, thecandidate);
       */

    //	ELOG_DEBUG("gathering done %u",stream_id);
    //ELOG_DEBUG("Candidates---------------------------------------------------->");
    while (lcands != NULL) {
      for (iterator = lcands; iterator; iterator = iterator->next) {
        char address[40], baseAddress[40];
        cand = (NiceCandidate*) iterator->data;
        nice_address_to_string(&cand->addr, address);
        nice_address_to_string(&cand->base_addr, baseAddress);
        if (strstr(address, ":") != NULL) {
          ELOG_DEBUG("Ignoring IPV6 candidate %s %p", address, this);
          continue;
        }
        //			ELOG_DEBUG("foundation %s", cand->foundation);
        //			ELOG_DEBUG("compid %u", cand->component_id);
        //			ELOG_DEBUG("stream_id %u", cand->stream_id);
        //			ELOG_DEBUG("priority %u", cand->priority);
        //			ELOG_DEBUG("username %s", cand->username);
        //			ELOG_DEBUG("password %s", cand->password);
        CandidateInfo cand_info;
        cand_info.componentId = cand->component_id;
        cand_info.foundation = cand->foundation;
        cand_info.priority = cand->priority;
        cand_info.hostAddress = std::string(address);
        cand_info.hostPort = nice_address_get_port(&cand->addr);
        cand_info.mediaType = mediaType;

        /*
         *   NICE_CANDIDATE_TYPE_HOST,
         * 	  NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE,
         *	  NICE_CANDIDATE_TYPE_PEER_REFLEXIVE,
         * 	  NICE_CANDIDATE_TYPE_RELAYED,
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
            char turnAddres[40];
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

        cand_info.username = std::string(ufrag);

        cand_info.password = std::string(upass);
        /*
           if (cand->username)
           cand_info.username = std::string(cand->username);
           else
           cand_info.username = std::string("(null)");

           if (cand->password)
           cand_info.password = std::string(cand->password);
           else
           cand_info.password = std::string("(null)");
           */

        localCandidates->push_back(cand_info);
      }
      lcands = nice_agent_get_local_candidates(agent_, stream_id,
          currentCompId++);
    }
    // According to libnice, this is how these must be free'd
    g_free(ufrag);
    g_free(upass);

    ELOG_INFO("candidate_gathering done with %lu candidates %p", localCandidates->size(), this);

    if (localCandidates->size()==0){
      ELOG_WARN("No local candidates found, check your network connection");
      exit(0);
    }
    updateIceState(NICE_CANDIDATES_GATHERED);
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
    }
    this->updateIceState(state);
  }

  IceState NiceConnection::checkIceState(){
    boost::unique_lock<boost::recursive_mutex> lock(stateMutex_);
    return iceState_;
  }

  void NiceConnection::updateIceState(IceState state) {
    boost::unique_lock<boost::recursive_mutex> lock(stateMutex_);
    if(iceState_==state)
      return;

    this->iceState_ = state;
    if (iceState_ == NICE_FINISHED) {
      return;
    }else if (iceState_ == NICE_FAILED){
      ELOG_WARN ("Ice Failed %p", this);
      this->listener_->updateIceState(iceState_,this);
      this->running_=false;
    }
    ELOG_DEBUG("%s - NICE State Changed %u %p", transportName->c_str(), state, this);
    this->iceState_ = state;
    if (state == NICE_READY){
      char ipaddr[30];
      NiceCandidate* local, *remote;
      nice_agent_get_selected_pair(agent_, 1, 1, &local, &remote); 
      nice_address_to_string(&local->addr, ipaddr);
      ELOG_DEBUG("Selected pair:\nlocal candidate addr: %s:%d",ipaddr, nice_address_get_port(&local->addr));
      nice_address_to_string(&remote->addr, ipaddr);
      ELOG_DEBUG("remote candidate addr: %s:%d",ipaddr, nice_address_get_port(&remote->addr));
    }
    lock.unlock();
    if (this->listener_ != NULL)
      this->listener_->updateIceState(state, this);
  }

} /* namespace erizo */
