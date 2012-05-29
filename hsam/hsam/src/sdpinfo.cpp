/*
 * SDPProcessor.cpp
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#include "sdpinfo.h"
#include <sstream>
#include <stdio.h>
#include <cstdlib>
#include <cstring>


#define CRYPTO = "a=crypto:"
#define CANDIDATE = "a=candidate:"
#define MID = "a=mid:"

using std::endl;

SDPInfo::SDPInfo() {
}

SDPInfo::~SDPInfo() {
	// TODO Auto-generated destructor stub
}

bool SDPInfo::initWithSDP(const std::string& sdp) {
	processSDP(sdp);
	return true;

}
void SDPInfo::addCandidate (const CandidateInfo &info){
	cand_vector.push_back(info);

}

void SDPInfo::addCrypto (const CryptoInfo &info){
	crypto_vector.push_back(info);
}

std::string SDPInfo::getSDP() {

	std::ostringstream sdp;
	sdp << "v=0\n"
			<< "o=- 0 0 IN IP4 127.0.0.1\n"
			<< "s=\n"
			<< "t=0 0\n";
	//candidates audio
	bool printedAudio = false, printedVideo = false;
	for (unsigned int it = 0 ; it<cand_vector.size(); it++){
		CandidateInfo cand = cand_vector[it];
		std::string hostType_str;
		switch (cand.type) {
		case HOST:
			hostType_str = "host";
			break;
		case SRLFX:
			hostType_str = "srflx";
			break;
		case PRFLX:
			hostType_str = "prflx";
			break;
		case RELAY:
			hostType_str = "relay";
			break;
		default:
			hostType_str = "host";
			break;
		}
		if (cand.media_type == AUDIO_TYPE){
			if (!printedAudio){
				sdp << "m=audio "<<cand.host_port << " RTP/AVPF 103 104 0 8 106 105 13 126\n"
				<< "c=IN IP4 " << cand.host_address <<endl
				<< "a=rtcp:"<<cand_vector[3].host_port<<" IN IP4 " << cand.host_address <<endl;
				printedAudio = true;
			}

			sdp << "a=candidate:" << cand.foundation << " " << cand.compid
					<< " " << cand.net_prot << " " << cand.priority << " "
					<< cand.host_address << " " << cand.host_port << " typ "
					<< hostType_str/* << " name " << cand.trans_prot << " network_name "
					<< "eth0 username " << cand.username << " password " << cand.passwd
					*/<< " generation 0" << endl;

			if(ice_username.empty()) {
				ice_username = cand.username;
				ice_passwd = cand.passwd;
			}
		}
	}
	//crypto audio
	if (printedAudio){
		sdp << "a=ice-ufrag:" << ice_username <<endl;
		sdp << "a=ice-pwd:" << ice_passwd <<endl;
		sdp << "a=mid:audio\na=rtcp-mux\n";
		for (unsigned int it = 0; it<crypto_vector.size(); it++){
			CryptoInfo cryp_info = crypto_vector[it];
			if (cryp_info.media_type==AUDIO_TYPE){
				sdp << "a=crypto:" << cryp_info.tag << " " << cryp_info.cipher_suite
						<< " " << "inline:" << cryp_info.key_params <<endl;
			}
		}

		sdp << "a=rtpmap:103 ISAC/16000\na=rtpmap:104 ISAC/32000\na=rtpmap:0 PCMU/8000\n"
				"a=rtpmap:8 PCMA/8000\na=rtpmap:106 CN/32000\na=rtpmap:105 CN/16000\n"
				"a=rtpmap:13 CN/8000\na=rtpmap:126 telephone-event/8000\n";
		sdp << "a=ssrc:"<<audio_ssrc <<" cname:o/i14u9pJrxRKAsu\na=ssrc:"<<audio_ssrc<<" mslabel:048f838f-2dd1-4a98-ab9e-8eb5f00abab8\na=ssrc:" << audio_ssrc << " label:iSight integrada\n";

	}

	for (unsigned int it = 0 ; it<cand_vector.size(); it++){
		CandidateInfo cand = cand_vector[it];
		std::string hostType_str;
		switch (cand.type) {
		case HOST:
			hostType_str = "host";
			break;
		case SRLFX:
			hostType_str = "srflx";
			break;
		case PRFLX:
			hostType_str = "prflx";
			break;
		case RELAY:
			hostType_str = "relay";
			break;
		default:
			hostType_str = "host";
			break;
		}
		if (cand.media_type == VIDEO_TYPE){
			if (!printedVideo){
				sdp << "m=video " <<cand.host_port << " RTP/AVPF 100 101 102\n"
						<< "c=IN IP4 " << cand.host_address <<endl
						<< "a=rtcp:"<<cand_vector[1].host_port << " IN IP4 " << cand.host_address <<endl;
				printedVideo = true;
			}

			sdp << "a=candidate:" << cand.foundation << " " << cand.compid
					<< " " << cand.net_prot << " " << cand.priority << " "
					<< cand.host_address << " " << cand.host_port << " typ "
					<< hostType_str/* << " name " << cand.trans_prot << " network_name "
					<< "eth0 username " << cand.username << " password " << cand.passwd
					*/<< " generation 0" << endl;

			if(ice_username.empty()) {
				ice_username = cand.username;
				ice_passwd = cand.passwd;
			}
		}
	}
	//crypto audio
	if (printedVideo){
		sdp << "a=ice-ufrag:" << ice_username <<endl;
		sdp << "a=ice-pwd:" << ice_passwd <<endl;
		sdp << "a=mid:video\na=rtcp-mux\n";
		for (unsigned int it = 0; it<crypto_vector.size(); it++){
			CryptoInfo cryp_info = crypto_vector[it];
			if (cryp_info.media_type==VIDEO_TYPE){
				sdp << "a=crypto:" << cryp_info.tag << " " << cryp_info.cipher_suite
						<< " " << "inline:" << cryp_info.key_params <<endl;
			}
		}

		sdp << "a=rtpmap:100 VP8/90000\na=rtpmap:101 red/90000\na=rtpmap:102 ulpfec/90000\n";
		sdp << "a=ssrc:"<<video_ssrc <<" cname:o/i14u9pJrxRKAsu\na=ssrc:"<<video_ssrc<<" mslabel:048f838f-2dd1-4a98-ab9e-8eb5f00abab8\na=ssrc:" << video_ssrc << " label:iSight integrada\n";


	}


	return sdp.str();
}

bool SDPInfo::processSDP(const std::string& sdp) {

	std::string strLine;
	std::istringstream iss(sdp);
	char* line= (char*)malloc(1000);
	char** pieces = (char**)malloc(10000);
	char** cryptopiece = (char**)malloc(5000);

	const char *cand = "a=candidate:";
	const char *crypto = "a=crypto:";
	//const char *mid = "a=mid:";
	const char *video = "m=video";
	const char *audio = "m=audio";
	const char *ice_user = "a=ice-ufrag";
	const char *ice_pass = "a=ice-pwd";
	mediaType mtype = OTHER;

	while(std::getline(iss,strLine)){
		const char* theline = strLine.c_str();
		sprintf(line, "%s\n", theline);
		char* isVideo = strstr(line,video);
		char* isAudio = strstr(line,audio);
		char* isCand = strstr(line,cand);
		char* isCrypt = strstr(line,crypto);
		char* isUser = strstr(line,ice_user);
		char* isPass = strstr(line,ice_pass);
//		char* ismid = strstr(line,mid);
		if (isVideo){
			mtype = VIDEO_TYPE;
		}
		if (isAudio){
			mtype = AUDIO_TYPE;
		}
		if(isCand!=NULL){
//			printf("cand %s\n", isCand );

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

			processCandidate (pieces,i-1, mtype);
		}
//		if(ismid!=NULL){
//			printf("MIIIIID %s\n", ismid+6);
//			if (!strcmp(ismid+6,"video")){
//
//			}else if(!strcmp(ismid+6,"audio")){
//
//			}
//
//		}
		if(isCrypt!=NULL){
		//	printf("crypt %s\n", isCrypt );
			CryptoInfo crypinfo;
			char *pch;
			pch = strtok (line," :");
			cryptopiece[0]=pch;
			int i = 0;
			while (pch != NULL)
			{
				pch = strtok (NULL, " :");
//				printf("cryptopiece %i es %s\n", i, pch);
				cryptopiece[i++]=pch;
			}

			crypinfo.cipher_suite = std::string(cryptopiece[1]);
			crypinfo.key_params = std::string(cryptopiece[3]);
			crypinfo.media_type = mtype;
			crypto_vector.push_back(crypinfo);
			//			sprintf(key, "%s",cryptopiece[3]);
			//				keys = g_slist_append(keys,key);
		}
		if(isUser){
			char *pch;
			pch = strtok (line,":");
			pch = strtok (NULL, ":");
			ice_username = std::string(pch);

		}
		if(isPass){
			char *pch;
			pch = strtok (line,":");
			pch = strtok (NULL, ":");
			ice_passwd = std::string(pch);
		}

	}
	free(line);
	free(pieces);
	free(cryptopiece);

	for(unsigned int i = 0; i < cand_vector.size(); i++) {
		CandidateInfo c = cand_vector[i];
		c.username = ice_username;
		c.passwd = ice_passwd;
	}

	return true;
}

std::vector<CandidateInfo>& SDPInfo::getCandidateInfos() {
	return cand_vector;
}

std::vector<CryptoInfo>& SDPInfo::getCryptoInfos() {
	return crypto_vector;
}

bool SDPInfo::processCandidate (char** pieces, int size, mediaType media_type){

	CandidateInfo cand;
	const char* types_str[10]={"host","srflx","prflx","relay"};
	cand.media_type = media_type;
	cand.foundation = pieces[0];
	cand.compid = (unsigned int)strtoul(pieces[1], NULL, 10);

	cand.net_prot = pieces[2];
//	if (strcmp(pieces[2],"udp")){
//		printf("error... como que no udp\n");
//	}
//	a=candidate:0 1 udp 2130706432 138.4.4.143 52314 typ host network_name en0 username F+pV4GqEWzOZWvno password U3KKQXo26W7RtukI5+SblDl8 generation 0
//		        0 1 2    3            4          5     6  7    8            9     10      11               12          13                     14       15
	cand.priority = (unsigned int)strtoul(pieces[3], NULL, 10);
	cand.host_address = std::string(pieces[4]);
	cand.host_port = (unsigned int)strtoul(pieces[5], NULL, 10);
	if (strcmp(pieces[6],"typ")){
		printf("error... aqui va typ, va %s \n", pieces[6]);
		return false;
	}
	unsigned int type=1111;
//	int offset=0;
	int p;
	for(p=0;p<4;p++){
		if (!strcmp(pieces[7],types_str[p])){
			type = p;
		}
	}
	switch (type) {
		case 0:
			cand.type = HOST;
			break;
		case 1:
			cand.type = SRLFX;
			break;
		case 2:
			cand.type = PRFLX;
			break;
		case 3:
			cand.type = RELAY;
			break;
		default:
			cand.type = HOST;
			break;
	}

	if (type==3){
		printf("tipo relay... metiendo offset");
//		offset=2;

		cand.relay_address = std::string(pieces[8]);
		cand.relay_port = (unsigned int)strtoul(pieces[9], NULL, 10);
	}
//	cand.trans_prot = pieces[9+offset];
	cand.trans_prot = "";
//	cand.username = std::string(pieces[11+offset]);
//	cand.passwd = std::string(pieces[13+offset]);
    cand_vector.push_back(cand);
	return true;
}



