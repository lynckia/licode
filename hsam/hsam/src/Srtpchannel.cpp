/*
 * Srtpchannel.cpp
 *
 *  Created on: Mar 8, 2012
 *      Author: pedro
 */

#include "Srtpchannel.h"
#include <srtp/srtp.h>
#include <nice/nice.h>
SrtpChannel::SrtpChannel() {
	srtp_init();
	active = false;
}

SrtpChannel::~SrtpChannel() {
	// TODO Auto-generated destructor stub
	if (send_session!=NULL){
		srtp_dealloc(send_session);
	}
	if (receive_session!=NULL){
		srtp_dealloc(receive_session);
	}

}

bool SrtpChannel::SetRtpParams(char* sendingKey, char* receivingKey){
	printf("Configuring srtp local key %s remote key %s\n", sendingKey, receivingKey);
	configureSRTPsession(&send_session, sendingKey, SENDING);
	configureSRTPsession(&receive_session,receivingKey, RECEIVING);
	sleep(1);
	active = true;

	return active;
}
bool SrtpChannel::SetRtcpParams(char* sendingKey, char* receivingKey){
	return 0;
}
int SrtpChannel::ProtectRtp(char* buffer, int *len){
	if (!active)
		return 0;
	int val = srtp_protect(send_session, buffer, len);
	if(val==0){
		return 0;
	}else{
		printf("Error SRTP %u\n",val);
		return -1;
	}
}
int SrtpChannel::UnprotectRtp(char* buffer, int *len){
	if(!active)
		return 0;
	rtcpheader *chead = (rtcpheader*)buffer;

	if(chead->packettype==200||chead->packettype==201){
//		printf("Es RTCP\n");
		*len=-1;
		return -1;
	}
	//		printf("Es RTP\n");
	int val = srtp_unprotect(receive_session, (char*)buffer, len);
	if(val==0){
		return 0;
	}else{
		printf("Error SRTP %u\n",val);
		return -1;
	}

}
int SrtpChannel::ProtectRtcp(char* buffer, int *len){
	int val = srtp_protect_rtcp(receive_session, (char*)buffer, len);
	if(val==0){
		return 0;
	}else{
		printf("Error SRTP %u\n",val);
		return -1;
	}
}
int SrtpChannel::UnprotectRtcp(char* buffer, int *len){
	int val = srtp_unprotect_rtcp(receive_session, buffer, len);
	if(val!=err_status_ok){
		return 0;
	}else{
		printf("Error SRTP %u\n",val);
		return -1;
	}
}
std::string SrtpChannel::generateBase64Key() {
	unsigned char key[30];
	crypto_get_random(key,30);
	gchar* base64key = g_base64_encode((guchar*)key, 30);
	return std::string(base64key);
}


bool SrtpChannel::configureSRTPsession(srtp_t *session, const char* key, enum Type type ){
	srtp_policy_t policy;
	memset(&policy, 0, sizeof(policy));

	//char aokey[100];
	// initialize libSRTP
//	if (!SrtpChannel::inited){
//		//		inited = true;
//	}

	// set policy to describe a policy for an SRTP stream
	// sprintf(key,"%s",theKey);
	crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
	crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	if (type == SENDING){

		policy.ssrc.type = ssrc_any_outbound;
	}else{

		policy.ssrc.type = ssrc_any_inbound;
	}
	policy.ssrc.value = 0;
	policy.window_size = 1024;
	policy.allow_repeat_tx = 1;
	policy.next = NULL;
	//printf("auth_tag_len %d\n", policy.rtp.auth_tag_len);

	gsize len = 0;
	uint8_t *akey = (uint8_t*)g_base64_decode ((gchar*)key,&len);
	printf("set master key/salt to %s/", octet_string_hex_string(akey, 16));
	printf("%s\n", octet_string_hex_string(akey+16, 14));
	// allocate and initialize the SRTP session
	policy.key = akey;
	int res = srtp_create(session, &policy);
//	return res!=0? false:true;
	return true;
}





