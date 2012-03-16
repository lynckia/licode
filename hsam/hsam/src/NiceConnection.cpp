/*
 * NiceConnection.cpp
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#include "NiceConnection.h"
#include "sdpinfo.h"
#include <stdio.h>

//

#include <glib.h>


guint stream_id;
GSList* lcands;
int streamsGathered, total_streams;
int rec, sen;
int length;
uint32_t ssrc = 55543;

void print_candidate_sdp (NiceCandidate *cand)
{

	char* types_str[10]={"host","srflx","prflx","relay"};

	char address[100];
	nice_address_to_string(&cand->addr, address);
	char* proto;
	switch (cand->stream_id){
	case 1:
		proto = "video_rtp";
		break;
	case 2:
		proto = "video_rtcp";
		break;
	case 3:
		proto = "rtp";
		break;
	case 4:
		proto = "rtcp";
		break;
	default:
		proto = "rtp";
		break;
	}
	if(cand->type == 3){
		char reladdr[100];
		printf("a=candidate:%s %u udp %u %s %u typ %s name %s network_name en1 username %s password %s generation 0\n",
				cand->foundation, cand->component_id, cand->priority,
				address, nice_address_get_port(&cand->addr), types_str[cand->type], proto, cand->username, cand->password);
	}else{
		printf("a=candidate:%s %u udp %u %s %u typ %s name %s network_name en1 username %s password %s generation 0\n",
				cand->foundation, cand->component_id, cand->priority,
				address, nice_address_get_port(&cand->addr), types_str[cand->type], proto, cand->username, cand->password);
	}
}

typedef struct
{
	uint32_t cc:4;
	uint32_t extension:1;
	uint32_t padding:1;
	uint32_t version:2;
	uint32_t payloadtype:7;
	uint32_t marker:1;
	uint32_t seqnum:16;
	uint32_t timestamp;
	uint32_t ssrc;
}rtpheader;


typedef struct
{
	uint32_t blockcount:5;
	uint32_t padding:1;
	uint32_t version:2;
	uint32_t packettype:8;
	uint32_t length:16;
	uint32_t ssrc;
}rtcpheader;

void cb_nice_recv( NiceAgent* agent, guint stream_id, guint component_id, guint len, gchar* buf, gpointer user_data )
{
	printf( "cb_nice_recv len %u id %u\n",len, stream_id );


}


void cb_candidate_gathering_done( NiceAgent *agent, guint stream_id, gpointer user_data )
{
	NiceConnection *conn = (NiceConnection*)user_data;
	conn->state = NiceConnection::CANDIDATES_GATHERED;
	printf("ConnState %u\n",conn->state);


	// ... Wait until the signal candidate-gathering-done is fired ...
	lcands = nice_agent_get_local_candidates( agent, stream_id, 1 );
	NiceCandidate *cand;
	GSList* iterator;
	//	printf("gathering done %u\n",stream_id);
	printf("Candidates---------------------------------------------------->\n");
	for (iterator = lcands; iterator; iterator=iterator->next){
		char address[100];
		cand = (NiceCandidate*)iterator->data;
		nice_address_to_string(&cand->addr, address);
		printf("foundation %s\n"), cand->foundation;
		printf("compid %u\n", cand->component_id);

		printf("stream_id %u\n",cand->stream_id);
		printf("priority %u\n", cand->priority);
		printf("username %s\n", cand->username);
		printf("password %s\n", cand->password);
//address, nice_address_get_port(&cand->addr)
		CandidateInfo cand_info;
		cand_info.compid = cand->component_id;
		cand_info.foundation = cand->foundation;
		cand_info.host_address = std::string(address);
		cand_info.host_port = nice_address_get_port(&cand->addr);
		cand_info.media_type = VIDEO_TYPE;
		cand_info.type = HOST;
		cand_info.net_prot= "udp";
		cand_info.trans_prot = "video_rtp";
		cand_info.net_name = "eth0";
		cand_info.username = std::string(cand->username);
		if(cand->password)
			cand_info.passwd = std::string(cand->password);
		else
			cand_info.passwd = std::string("(null)");

		conn->localCandidates.push_back(cand_info);

//		print_candidate_sdp(cand);
	}
	printf("candidate_gathering done\n");

}

void cb_component_state_changed( void )
{
	printf( "cb_component_state_changed\n" );
}


void cb_new_selected_pair  (NiceAgent *agent, guint stream_id, guint component_id, gchar *lfoundation, gchar *rfoundation, gpointer user_data)
{
	printf( "cb_new_selected_pair for stream %u, comp %u, lfound %s, rfound %s\n", stream_id, component_id, lfoundation, rfoundation );
}


NiceConnection::NiceConnection(const std::string &localaddr, const std::string &stunaddr) {
	streamsGathered = 0;
	total_streams = 0;
	state = INITIAL;

	g_type_init();
	g_thread_init( NULL );

	loop = g_main_loop_new( NULL, FALSE );

	//	nice_debug_enable( TRUE );

	// Create a nice agent
	agent = nice_agent_new( g_main_loop_get_context( loop ),NICE_COMPATIBILITY_GOOGLE);

	NiceAddress* naddr = nice_address_new();

	//nice_address_set_from_string(naddr,"138.4.4.141");
	nice_address_set_from_string(naddr,localaddr.c_str());
	nice_agent_add_local_address (agent, naddr);

	GValue val = { 0 }, val2 = { 0 };

	g_value_init( &val, G_TYPE_STRING );
	//g_value_set_string( &val, "173.194.70.126" );
	g_value_set_string( &val, stunaddr.c_str() );
	g_object_set_property( G_OBJECT( agent ), "stun-server", &val );

	g_value_init( &val2, G_TYPE_UINT );
	g_value_set_uint( &val2, 19302 );
	g_object_set_property( G_OBJECT( agent ), "stun-server-port", &val2 );

	// Connect the signals
	g_signal_connect( G_OBJECT( agent ), "candidate-gathering-done", G_CALLBACK( cb_candidate_gathering_done ), this );
	g_signal_connect( G_OBJECT( agent ), "component-state-changed", G_CALLBACK( cb_component_state_changed ), NULL );
	g_signal_connect( G_OBJECT( agent ), "new-selected-pair", G_CALLBACK( cb_new_selected_pair ), NULL );

	// Create a new stream with one component and start gathering candidates

	nice_agent_add_stream( agent, 1);
	nice_agent_gather_candidates(agent, 1);
	nice_agent_attach_recv( agent, 1, 1, g_main_loop_get_context( loop ), cb_nice_recv, NULL );


	// Attach to the component to receive the data

	g_main_loop_run( loop );

	// Destroy the object
	state = FINISHED;
	g_object_unref( agent );

}



NiceConnection::~NiceConnection() {
	// TODO Auto-generated destructor stub
}

bool NiceConnection::setRemoteCandidates(std::vector<CandidateInfo> &candidates){
	return true;
}

//std::vector<CandidateInfo> NiceConnection::getLocalCandidates(){
//	std::vector<CandidateInfo> vic;
//	return vic;
//}









