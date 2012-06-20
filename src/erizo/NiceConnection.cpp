/*
 * NiceConnection.cpp
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#include <glib.h>
#include <nice/nice.h>

#include "NiceConnection.h"
#include "WebRtcConnection.h"
#include "SdpInfo.h"

namespace erizo {

guint stream_id;
GSList* lcands;
int streamsGathered;
int rec, sen;
int length;
int components = 2;
uint32_t ssrc = 55543;
boost::mutex writeMutex, gatherMutex, stateMutex, selectedMutex;

void cb_nice_recv(NiceAgent* agent, guint stream_id, guint component_id,
		guint len, gchar* buf, gpointer user_data) {

	boost::mutex::scoped_lock lock(writeMutex);
//	printf( "cb_nice_recv len %u id %u\n",len, stream_id );
	NiceConnection* nicecon = (NiceConnection*) user_data;
	nicecon->getWebRtcConnection()->receiveNiceData((char*) buf, (int) len,
			(NiceConnection*) user_data);
}

void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id,
		gpointer user_data) {

	boost::mutex::scoped_lock lock(gatherMutex);
	NiceConnection *conn = (NiceConnection*) user_data;
	//printf("ConnState %u\n",conn->state);
	// ... Wait until the signal candidate-gathering-done is fired ...
	int currentCompId = 1;
	lcands = nice_agent_get_local_candidates(agent, stream_id, currentCompId++);
	NiceCandidate *cand;
	GSList* iterator;
	//	printf("gathering done %u\n",stream_id);
	//printf("Candidates---------------------------------------------------->\n");
	while (lcands != NULL) {
		for (iterator = lcands; iterator; iterator = iterator->next) {
			char address[40];
			cand = (NiceCandidate*) iterator->data;
			nice_address_to_string(&cand->addr, address);
			if (strstr(address, ":")!=NULL){
				printf("Ignoring IPV6 candidate\n");
				continue;

			}
//			printf("foundation %s\n", cand->foundation);
//			printf("compid %u\n", cand->component_id);
//			printf("stream_id %u\n", cand->stream_id);
//			printf("priority %u\n", cand->priority);
//			printf("username %s\n", cand->username);
//			printf("password %s\n", cand->password);
			CandidateInfo cand_info;
			cand_info.componentId = cand->component_id;
			cand_info.foundation = cand->foundation;
			cand_info.priority = cand->priority;
			cand_info.hostAddress = std::string(address);
			cand_info.hostPort = nice_address_get_port(&cand->addr);
			cand_info.mediaType = conn->mediaType;


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
				printf("WARNING TURN NOT IMPLEMENTED YET\n");
				cand_info.hostType = RELAY;
				break;
			default:
				break;
			}
			cand_info.netProtocol = "udp";
			cand_info.transProtocol = std::string(*conn->transportName);
			//cand_info.username = std::string(cand->username);
			if (cand->username)
				cand_info.username = std::string(cand->username);
			else
				cand_info.username = std::string("(null)");

			if (cand->password)
				cand_info.password = std::string(cand->password);
			else
				cand_info.password = std::string("(null)");

			conn->localCandidates->push_back(cand_info);
		}
		lcands = nice_agent_get_local_candidates(agent, stream_id,
				currentCompId++);
	}
	printf("candidate_gathering done, size %u\n",
			conn->localCandidates->size());
	conn->iceState = NiceConnection::CANDIDATES_GATHERED;
}

void cb_component_state_changed(void) {

	boost::mutex::scoped_lock lock(stateMutex);
	printf("cb_component_state_changed\n");
}

void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id,
		gchar *lfoundation, gchar *rfoundation, gpointer user_data) {
	printf(
			"cb_new_selected_pair for stream %u, comp %u, lfound %s, rfound %s OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO\n",
			stream_id, component_id, lfoundation, rfoundation);
	boost::mutex::scoped_lock lock(selectedMutex);
	NiceConnection *conn = (NiceConnection*) user_data;
	conn->iceState = NiceConnection::READY;
	printf(
			"cb_new_selected_pair for stream %u, comp %u, lfound %s, rfound %s OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO\n",
			stream_id, component_id, lfoundation, rfoundation);
}

NiceConnection::NiceConnection(MediaType med,
		const std::string &transport_name) {

	agent_ = NULL;
	loop_ = NULL;
	mediaType = med;
	localCandidates = new std::vector<CandidateInfo>();
	transportName = new std::string(transport_name);
}

NiceConnection::~NiceConnection() {

	if (iceState != FINISHED)
		this->close();
	if (agent_)
		g_object_unref(agent_);
	if (localCandidates)
		delete localCandidates;
	if (transportName)
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
	iceState = FINISHED;
}

int NiceConnection::sendData(void *buf, int len) {

	int val = -1;
	if (iceState == READY) {
		val = nice_agent_send(agent_, 1, 1, len, (char*) buf);
	}
	return val;
}

WebRtcConnection* NiceConnection::getWebRtcConnection() {

	return conn_;
}

void NiceConnection::init() {

	streamsGathered = 0;
	iceState = INITIAL;

	g_type_init();
	g_thread_init(NULL);

	loop_ = g_main_loop_new(NULL, FALSE);
	//	nice_debug_enable( TRUE );
	// Create a nice agent
	agent_ = nice_agent_new(g_main_loop_get_context(loop_),
			NICE_COMPATIBILITY_GOOGLE);

//	NiceAddress* naddr = nice_address_new();
//	nice_address_set_from_string(naddr, "138.4.4.141");
//	nice_agent_add_local_address(agent_, naddr);

	GValue val = { 0 }, val2 = { 0 };

	g_value_init(&val, G_TYPE_STRING);
	g_value_set_string(&val, "173.194.70.126");
	g_object_set_property(G_OBJECT( agent_ ), "stun-server", &val);

	g_value_init(&val2, G_TYPE_UINT);
	g_value_set_uint(&val2, 19302);
	g_object_set_property(G_OBJECT( agent_ ), "stun-server-port", &val2);

	// Connect the signals
	g_signal_connect( G_OBJECT( agent_ ), "candidate-gathering-done",
			G_CALLBACK( cb_candidate_gathering_done ), this);
	g_signal_connect( G_OBJECT( agent_ ), "component-state-changed",
			G_CALLBACK( cb_component_state_changed ), NULL);
	g_signal_connect( G_OBJECT( agent_ ), "new-selected-pair",
			G_CALLBACK( cb_new_selected_pair ), this);

	// Create a new stream and start gathering candidates

	int res = nice_agent_add_stream(agent_, 1);
	// Set Port Range ----> If this doesn't work when linking the file libnice.sym has to be modified to include this call
//	nice_agent_set_port_range(agent_, (guint)1, (guint)1, (guint)51000, (guint)52000);

	nice_agent_gather_candidates(agent_, 1);
	nice_agent_attach_recv(agent_, 1, 1, g_main_loop_get_context(loop_),
			cb_nice_recv, this);

	// Attach to the component to receive the data
	g_main_loop_run(loop_);
}

bool NiceConnection::setRemoteCandidates(
		std::vector<CandidateInfo> &candidates) {

	GSList* candList = NULL;

	for (unsigned int it = 0; it < candidates.size(); it++) {
		NiceCandidateType nice_cand_type;
		CandidateInfo cinfo = candidates[it];
		if (cinfo.mediaType != this->mediaType
				|| this->transportName->compare(cinfo.transProtocol))
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
		char* uname = (char*) malloc(cinfo.username.size());
		char* pass = (char*) malloc(cinfo.password.size());
		sprintf(thecandidate->foundation, "%s", cinfo.foundation.c_str());
		sprintf(uname, "%s", cinfo.username.c_str());
		sprintf(pass, "%s", cinfo.password.c_str());

		thecandidate->username = uname;
		thecandidate->password = pass;
		thecandidate->stream_id = (guint) 1;
		thecandidate->component_id = cinfo.componentId;
		thecandidate->priority = cinfo.priority;
		thecandidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
		candList = g_slist_append(candList, thecandidate);

	}
	nice_agent_set_remote_candidates(agent_, (guint) 1, 1, candList);
	printf("Candidates SET \n");
	iceState = CANDIDATES_RECEIVED;
	return true;
}

void NiceConnection::setWebRtcConnection(WebRtcConnection* connection) {

	this->conn_ = connection;
}

} /* namespace erizo */
