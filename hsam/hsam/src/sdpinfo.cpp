/*
 * SDPProcessor.cpp
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#include "sdpinfo.h"
#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <nice/nice.h>

#define CRYPTO="a=crypto:"
#define CANDIDATE = "a=candidate:"
#define MID = "a=mid:"

SDPInfo::SDPInfo() {
	printf("hola SDPProcessor \n");
}

SDPInfo::~SDPInfo() {
	// TODO Auto-generated destructor stub
}

bool SDPInfo::initWithSDP(const std::string& sdp) {
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

bool SDPInfo::processSDP(const std::string& sdp) {

	std::string strLine;
	std::istringstream iss(sdp);
	char* line= (char*)malloc(1000);
	char** pieces = (char**)malloc(10000);
	char** cryptopiece = (char**)malloc(5000);

	const char *cand = "a=candidate:";
	const char *crypto = "a=crypto:";
	const char *mid = "a=mid:";
	while(std::getline(iss,strLine)){
		const char* theline = strLine.c_str();
		sprintf(line, "%s\n", theline);

		char* isCand = strstr(line,cand);
		char* isCrypt = strstr(line,crypto);
		char* ismid = strstr(line,mid);
		if(isCand!=NULL){
			printf("cand %s\n", isCand );

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

			processCandidate (pieces,i-1);

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

	}
	free(pieces);
	free(cryptopiece);


	return true;
}

void SDPInfos::processCandidate (char** pieces, int size){

	const char* types_str[10]={"host","srflx","prflx","relay"};

	CandidateInfo *cand = new CandidateInfo();
	cand->foundation = pieces[0];
	cand->compid = (unsigned int)strtoul(pieces[1], NULL, 10);

	cand->net_prot = pieces[2];
//	if (strcmp(pieces[2],"udp")){
//		printf("error... como que no udp\n");
//	}
	cand->priority = (unsigned int)strtoul(pieces[3], NULL, 10);
	cand->host_address = std::string(pieces[4]);
	cand->host_port = (unsigned int)strtoul(pieces[5], NULL, 10);
	if (strcmp(pieces[6],"typ")){
		printf("error... aqui va typ\n");
	}
	int type=-1;
	int offset=0;
	int p;
	for(p=0;p<4;p++){
		if (!strcmp(pieces[7],types_str[p])){
			type = p;
		}
	}
	if (type==3){
		printf("tipo relay... metiendo offset");
		offset=2;

		cand->relay_address = std::string(pieces[8]);
		cand->relay_port = (unsigned int)strtoul(pieces[9], NULL, 10);
	}
	cand->net_prot = pieces[9+offset];
	cand->username = std::string(pieces[13+offset]);
	cand->passwd = std::string(pieces[15+offset]);


	//return cand;
}



