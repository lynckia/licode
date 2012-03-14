/*
 * SDPProcessor.cpp
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#include "SDPProcessor.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream>

#define CRYPTO="a=crypto:"
#define CANDIDATE = "a=candidate:"
#define MID = "a=mid:"

SDPProcessor::SDPProcessor() {
	printf("hola SDPProcessor \n");
}

SDPProcessor::~SDPProcessor() {
	// TODO Auto-generated destructor stub
}

bool SDPProcessor::initWithSDP(const std::string& sdp) {
	processSDP(sdp);
	return true;

}

const std::string& SDPProcessor::getSDP() {
	/*
	char* types_str[10]={"host","srflx","prflx","relay"};

	char address[100];
	nice_address_to_string(&cand->addr, address);
	char* proto;
	switch (cand->stream_id){
		case id_video_rtp:
			proto = "video_rtp";
			break;
		case id_video_rtcp:
			proto = "video_rtcp";
			break;
		case id_audio_rtp:
			proto = "rtp";
			break;
		case id_audio_rtcp:
			proto = "rtcp";
			break;
		default:
			proto = "rtp";
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
*/
	return "hola";

}

bool SDPProcessor::processSDP(const std::string& sdp) {

	std::string strLine;
	std::istringstream iss(sdp);
	char* line;
	char** pieces = (char**)malloc(10000);
	char** cryptopiece = (char**)malloc(5000);
	char *cand = "a=candidate:";
	char *crypto = "a=crypto:";
	char *mid = "a=mid:";
	while(std::getline(iss,strLine)){
		sprintf(line, "%s", strLine.c_str());

		char* isCand = strstr(line,cand);
		char* isCrypt = strstr(line,crypto);
		char* ismid = strstr(line,mid);
		if(isCand!=NULL){
			printf("cand %s\n", isCrypt );

			char *pch;
			pch = strtok (line," :");
			pieces[0] = pch;
			int i = 0;
			while (pch != NULL)
			{
				//printf ("%s\n",pch);
				pch = strtok (NULL, " :");
				pieces[i++]=pch;
			}

			//candidate = processCandidate (pieces,i-1);
			//candList = g_slist_append(candList,candidate);
		}
		if(ismid!=NULL){
			char* pch;
			printf("MIIIIID %s\n", ismid+6);
			if (!strcmp(ismid+6,"video")){

			}else if(!strcmp(ismid+6,"audio")){

			}

		}
		if(isCrypt!=NULL){
			printf("crypt %s\n", isCrypt );
			char *pch;
			pch = strtok (line," :");
			cryptopiece[0]=pch;
			int i = 0;
			while (pch != NULL)
			{
				pch = strtok (NULL, " :");
				cryptopiece[i++]=pch;
			}

			//			sprintf(key, "%s",cryptopiece[3]);
			//				keys = g_slist_append(keys,key);
		}
		free(pieces);
		free(cryptopiece);

	}

	return true;
}


