/*
 * NiceConnection.cpp
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#include "NiceConnection.h"
#include "WebRTCConnection.h"
#include "sdpinfo.h"
#include <glib.h>

guint stream_id;
GSList* lcands;
int streamsGathered, total_streams;
int rec, sen;
int length;
uint32_t ssrc = 55543;
boost::mutex write_mutex, gather_mutex, state_mutex, selected_mutex;

void cb_nice_recv( NiceAgent* agent, guint stream_id, guint component_id, guint len, gchar* buf, gpointer user_data )
{
	boost::mutex::scoped_lock lock(write_mutex);

//	printf( "cb_nice_recv len %u id %u\n",len, stream_id );
	NiceConnection* nicecon = (NiceConnection*)user_data;
	nicecon->getWebRTCConnection()->receiveNiceData((char*)buf,(int)len, (NiceConnection*)user_data);
}


void cb_candidate_gathering_done( NiceAgent *agent, guint stream_id, gpointer user_data )
{
	boost::mutex::scoped_lock lock(gather_mutex);

	NiceConnection *conn = (NiceConnection*)user_data;
	//printf("ConnState %u\n",conn->state);


	// ... Wait until the signal candidate-gathering-done is fired ...
	lcands = nice_agent_get_local_candidates( agent, stream_id, 1 );
	NiceCandidate *cand;
	GSList* iterator;
	//	printf("gathering done %u\n",stream_id);
	//printf("Candidates---------------------------------------------------->\n");
	for (iterator = lcands; iterator; iterator=iterator->next){
		char address[100];
		cand = (NiceCandidate*)iterator->data;
		nice_address_to_string(&cand->addr, address);
//		printf("foundation %s\n", cand->foundation);
//		printf("compid %u\n", cand->component_id);
//
//		printf("stream_id %u\n",cand->stream_id);
//		printf("priority %u\n", cand->priority);
//		printf("username %s\n", cand->username);
//		printf("password %s\n", cand->password);
		CandidateInfo cand_info;
		cand_info.compid = cand->component_id;
		cand_info.foundation = cand->foundation;
		cand_info.priority = (float)cand->priority;
		cand_info.host_address = std::string(address);
		cand_info.host_port = nice_address_get_port(&cand->addr);
		cand_info.media_type = conn->media_type;

		/*
		 *   NICE_CANDIDATE_TYPE_HOST,
  	  	 * 	  NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE,
  	  	 *	  NICE_CANDIDATE_TYPE_PEER_REFLEXIVE,
  	  	 * 	  NICE_CANDIDATE_TYPE_RELAYED,
		 */
		switch (cand->type) {
			case NICE_CANDIDATE_TYPE_HOST :
				cand_info.type = HOST;
				break;
			case NICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
				cand_info.type = SRLFX;
				break;
			case NICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
				cand_info.type = PRFLX;
				break;
			case NICE_CANDIDATE_TYPE_RELAYED:
				printf("WARNING TURN NOT IMPLEMENTED YET\n");
				cand_info.type = RELAY;
				break;
			default:
				break;
		}
		cand_info.net_prot= "udp";
		cand_info.trans_prot = std::string(*conn->trans_name);
		if(!conn->trans_name->compare("video_rtcp")||!conn->trans_name->compare("rtcp")){
			cand_info.compid =2;
		}
		cand_info.net_name = "eth0";
		cand_info.username = std::string(cand->username);
		if(cand->password)
			cand_info.passwd = std::string(cand->password);
		else
			cand_info.passwd = std::string("(null)");

		conn->local_candidates->push_back(cand_info);

//		print_candidate_sdp(cand);
	}
	printf("candidate_gathering done, size %u\n", conn->local_candidates->size());
	conn->state = NiceConnection::CANDIDATES_GATHERED;

}

void cb_component_state_changed( void )
{
	boost::mutex::scoped_lock lock(state_mutex);

	printf( "cb_component_state_changed\n" );
}


void cb_new_selected_pair  (NiceAgent *agent, guint stream_id, guint component_id, gchar *lfoundation, gchar *rfoundation, gpointer user_data)
{
	boost::mutex::scoped_lock lock(selected_mutex);
	NiceConnection *conn = (NiceConnection*)user_data;
	conn->state = NiceConnection::READY;


	printf( "cb_new_selected_pair for stream %u, comp %u, lfound %s, rfound %s\n", stream_id, component_id, lfoundation, rfoundation );
}


NiceConnection::NiceConnection(mediaType med, const std::string &transport_name) {
	agent = NULL;
	loop = NULL;
	media_type = med;
	local_candidates = new std::vector<CandidateInfo>();
	trans_name = new std::string (transport_name);

}

NiceConnection::~NiceConnection() {
	if(state!=FINISHED)
		this->close();
	if (agent)
		g_object_unref(agent);
	if (local_candidates)
		delete local_candidates;
	if (trans_name)
		delete trans_name;
	// TODO Auto-generated destructor stub

}

void NiceConnection::join() {
	m_Thread.join();
}

void NiceConnection::start() {
	m_Thread = boost::thread(&NiceConnection::init, this);
}

void NiceConnection::close() {
	if (agent!=NULL)
		nice_agent_remove_stream(agent,1);
	if (loop!=NULL)
		g_main_loop_quit(loop);
	state = FINISHED;

}


int NiceConnection::sendData(void *buf, int len){
	int val = -1;
	if (state == READY){
		val = nice_agent_send(agent, 1, 1, len, (char*)buf);
	}
	return val;
}
WebRTCConnection* NiceConnection::getWebRTCConnection() {
	return conn;
}

void NiceConnection::init(){
	streamsGathered = 0;
	total_streams = 0;
	state = INITIAL;

	g_type_init();
	g_thread_init( NULL );

	loop = g_main_loop_new( NULL, FALSE );

	//	nice_debug_enable( TRUE );

	// Create a nice agent
	agent = nice_agent_new( g_main_loop_get_context( loop ),NICE_COMPATIBILITY_GOOGLE);

	GValue val = { 0 }, val2 = { 0 };

	g_value_init( &val, G_TYPE_STRING );
	g_value_set_string( &val, "173.194.70.126" );
	//g_value_set_string( &val, stunaddr.c_str() );
	g_object_set_property( G_OBJECT( agent ), "stun-server", &val );

	g_value_init( &val2, G_TYPE_UINT );
	g_value_set_uint( &val2, 19302 );
	g_object_set_property( G_OBJECT( agent ), "stun-server-port", &val2 );

	// Connect the signals
	g_signal_connect( G_OBJECT( agent ), "candidate-gathering-done", G_CALLBACK( cb_candidate_gathering_done ), this );
	g_signal_connect( G_OBJECT( agent ), "component-state-changed", G_CALLBACK( cb_component_state_changed ), NULL );
	g_signal_connect( G_OBJECT( agent ), "new-selected-pair", G_CALLBACK( cb_new_selected_pair ), this );

	// Create a new stream with one component and start gathering candidates

	int res = nice_agent_add_stream( agent, 1);
	// Set Port Range ----> If this doesn't work when linking the file libnice.sym has to be modified to include this call
	nice_agent_set_port_range(agent, (guint)1, (guint)1, (guint)51000, (guint)52000);
	printf("EL ID ES %d\n", res);

	nice_agent_gather_candidates(agent, 1);
	nice_agent_attach_recv( agent, 1, 1, g_main_loop_get_context( loop ), cb_nice_recv, this );


	// Attach to the component to receive the data

	g_main_loop_run( loop );


}


bool NiceConnection::setRemoteCandidates(std::vector<CandidateInfo> &candidates){
	GSList* candList = NULL;

	for (unsigned int it =0; it<candidates.size(); it++){
		NiceCandidateType nice_cand_type;
		CandidateInfo cinfo = candidates[it];
		if (cinfo.media_type != this->media_type||this->trans_name->compare(cinfo.trans_prot))
			continue;

		switch (cinfo.type) {
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
		//138.4.4.141

		NiceCandidate* thecandidate = nice_candidate_new(nice_cand_type);
		NiceAddress* naddr = nice_address_new();
		nice_address_set_from_string(naddr, cinfo.host_address.c_str());
		nice_address_set_port(naddr, cinfo.host_port);
		thecandidate->addr = *naddr;
		char* uname = (char*)malloc(cinfo.username.size());
		char* pass = (char*)malloc(cinfo.passwd.size());
		sprintf(thecandidate->foundation,"%s", cinfo.foundation.c_str());
		sprintf(uname,"%s", cinfo.username.c_str());
		sprintf(pass,"%s", cinfo.passwd.c_str());
		thecandidate->username = uname;
		thecandidate->password = pass;
		thecandidate->stream_id = (guint)1;
		thecandidate->component_id = cinfo.compid;
		thecandidate->priority = cinfo.priority;
		thecandidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
		candList = g_slist_append(candList,thecandidate);

	}
	nice_agent_set_remote_candidates(agent, (guint)1, 1, candList);
	printf("Candidates SET \n");
	state = CANDIDATES_RECEIVED;

	return true;
}

void NiceConnection::setWebRTCConnection(WebRTCConnection *connection){
	this->conn = connection;
}
