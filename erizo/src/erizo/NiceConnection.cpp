/*
 * NiceConnection.cpp
 */

#include <glib.h>
#include <nice/nice.h>
 #include <cstdio>

#include "NiceConnection.h"
#include "SdpInfo.h"

namespace erizo {
	DEFINE_LOGGER(NiceConnection, "NiceConnection");

guint stream_id;
GSList* lcands;
int streamsGathered;
int rec, sen;
int length;

void cb_nice_recv(NiceAgent* agent, guint stream_id, guint component_id,
		guint len, gchar* buf, gpointer user_data) {

	NiceConnection* nicecon = (NiceConnection*) user_data;
	nicecon->getNiceListener()->onNiceData(component_id, reinterpret_cast<char*> (buf), static_cast<unsigned int> (len),
			(NiceConnection*) user_data);
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
		//conn->getWebRtcConnection()->close();
	}

}

void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
		gchar *lfoundation, gchar *rfoundation, gpointer user_data) {
	NiceConnection *conn = (NiceConnection*) user_data;
	conn->updateComponentState(component_id, NICE_READY);
}

NiceConnection::NiceConnection(MediaType med,
		const std::string &transport_name, unsigned int iceComponents, const std::string& stunServer,
    	int stunPort, int minPort, int maxPort):mediaType(med), iceComponents_(iceComponents), 
    	stunServer_(stunServer), stunPort_ (stunPort), minPort_(minPort), maxPort_(maxPort) {
	agent_ = NULL;
	loop_ = NULL;
	listener_ = NULL;
	localCandidates = new std::vector<CandidateInfo>();
	transportName = new std::string(transport_name);
	for (unsigned int i = 1; i<=iceComponents; i++) {
		comp_state_list[i] = NICE_INITIAL;
	}

}

NiceConnection::~NiceConnection() {
  ELOG_DEBUG("NiceConnection Destructor");
	if (iceState != NICE_FINISHED)
		this->close();
	if (agent_!=NULL){
		g_object_unref(agent_);
    	agent_ = NULL;
  	}
  	if (loop_ != NULL) {
  		g_main_loop_unref (loop_);
  	}
	if (localCandidates!=NULL)
		delete localCandidates;
	if (transportName!=NULL)
		delete transportName;
}

void NiceConnection::join() {

	m_Thread_.join();
}

void NiceConnection::start() {

	m_Thread_ = boost::thread(&NiceConnection::init, this);
}

void NiceConnection::close() {
	if (agent_ != NULL)
		nice_agent_remove_stream(agent_, 1);
	if (loop_ != NULL)
		g_main_loop_quit(loop_);
	iceState = NICE_FINISHED;
}

int NiceConnection::sendData(unsigned int compId, const void* buf, int len) {

	int val = -1;
	if (iceState == NICE_READY) {
		val = nice_agent_send(agent_, 1, compId, len, reinterpret_cast<const gchar*>(buf));
	}
	if (val != len) {
		ELOG_DEBUG("Data sent %d of %d", val, len);
	}
	return val;
}

void NiceConnection::init() {

	streamsGathered = 0;
	this->updateIceState(NICE_INITIAL);

	g_type_init();
	ELOG_DEBUG("Creating Main Context");
	context_ = g_main_context_new();
	ELOG_DEBUG("Creating Main Loop");
	loop_ =  g_main_loop_new(context_, FALSE);
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

	ELOG_WARN("Stun Server %s:%d", stunServer_.c_str(), stunPort_);
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
  	nice_agent_set_port_range(agent_, (guint)1, (guint)1, (guint)minPort_, (guint)maxPort_);
  }

	nice_agent_gather_candidates(agent_, 1);
	nice_agent_attach_recv(agent_, 1, 1, context_,
			cb_nice_recv, this);
	if (iceComponents_ > 1) {
		nice_agent_attach_recv(agent_, 1, 2, context_,
			cb_nice_recv, this);
	}

	// Attach to the component to receive the data
	g_main_loop_run(loop_);
}

bool NiceConnection::setRemoteCandidates(
		std::vector<CandidateInfo> &candidates) {

	ELOG_DEBUG("Setting remote candidates %d", candidates.size());


	for (unsigned int compId = 1; compId <= iceComponents_; compId++) {

		GSList* candList = NULL;

		for (unsigned int it = 0; it < candidates.size(); it++) {
			NiceCandidateType nice_cand_type;
			CandidateInfo cinfo = candidates[it];
			if (cinfo.mediaType != this->mediaType
					|| this->transportName->compare(cinfo.transProtocol)
					|| cinfo.componentId != compId)
				continue;

			switch (cinfo.hostType) {
			case HOST:
				nice_cand_type = NICE_CANDIDATE_TYPE_HOST;
				break;
			case SRLFX:
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
			NiceAddress* naddr = nice_address_new();
			nice_address_set_from_string(naddr, cinfo.hostAddress.c_str());
			nice_address_set_port(naddr, cinfo.hostPort);
			thecandidate->addr = *naddr;
			sprintf(thecandidate->foundation, "%s", cinfo.foundation.c_str());

			thecandidate->username = strdup(cinfo.username.c_str());
			thecandidate->password = strdup(cinfo.password.c_str());
			thecandidate->stream_id = (guint) 1;
			thecandidate->component_id = cinfo.componentId;
			thecandidate->priority = cinfo.priority;
			thecandidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
			candList = g_slist_append(candList, thecandidate);
			ELOG_DEBUG("New Candidate SET %s %d", cinfo.hostAddress.c_str(), cinfo.hostPort);

		}
		nice_agent_set_remote_candidates(agent_, (guint) 1, compId, candList);
	}

	ELOG_DEBUG("Candidates SET");
	this->updateIceState(NICE_CANDIDATES_RECEIVED);
	return true;
}

void NiceConnection::gatheringDone(uint stream_id) {
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
			char address[40];
			cand = (NiceCandidate*) iterator->data;
			nice_address_to_string(&cand->addr, address);
			if (strstr(address, ":") != NULL) {
				ELOG_DEBUG("Ignoring IPV6 candidate %s", address);
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
				cand_info.hostType = SRLFX;
				break;
			case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
				cand_info.hostType = PRFLX;
				break;
			case NICE_CANDIDATE_TYPE_RELAYED:
				ELOG_DEBUG("WARNING TURN NOT IMPLEMENTED YET");
				cand_info.hostType = RELAY;
				break;
			default:
				break;
			}
			cand_info.netProtocol = "udp";
			cand_info.transProtocol = std::string(*transportName);

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
	ELOG_INFO("candidate_gathering done with %d candidates", localCandidates->size());

	if (localCandidates->size()==0){
	    ELOG_WARN("No local candidates found, check your network connection");
    	exit(0);
  	}
	updateIceState(NICE_CANDIDATES_GATHERED);
}

void NiceConnection::setNiceListener(NiceConnectionListener *listener) {
	this->listener_ = listener;
}

NiceConnectionListener* NiceConnection::getNiceListener() {
	return this->listener_;
}

void NiceConnection::updateComponentState(unsigned int compId, IceState state) {
	ELOG_DEBUG("NICE Component State Changed %u - %u", compId, state);
	comp_state_list[compId] = state;
	if (state == NICE_READY) {
		for (unsigned int i = 1; i<=iceComponents_; i++) {
			if (comp_state_list[i] != NICE_READY) {
				return;
			}
		}
	}
	this->updateIceState(state);
}

void NiceConnection::updateIceState(IceState state) {
	ELOG_DEBUG("NICE State Changed %u", state);
	this->iceState = state;
	if (this->listener_ != NULL)
		this->listener_->updateIceState(state, this);
}

} /* namespace erizo */
