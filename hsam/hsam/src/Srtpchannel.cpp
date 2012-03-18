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

}

SrtpChannel::~SrtpChannel() {
	// TODO Auto-generated destructor stub
	if (*send_session!=NULL){
		srtp_dealloc(*send_session);
	}
	if (*receive_session!=NULL){
		srtp_dealloc(*receive_session);
	}

}

bool SrtpChannel::SetRtpParams(char* sendingKey, char* receivingKey){
	if (configureSRTPsession(send_session, sendingKey, SENDING) && configureSRTPsession(receive_session,receivingKey, RECEIVING))
		return true;
	return false;
}
bool SrtpChannel::SetRtcpParams(char* sendingKey, char* receivingKey){
	return 0;
}
int SrtpChannel::ProtectRtp(void* buffer, int len){
	int val = srtp_protect(*receive_session, buffer, &len)==0;
	if(val==0){
		return len;
	}else{
		printf("Error SRTP %u\n",val);
		return val;
	}
	return len;
}
int SrtpChannel::UnprotectRtp(void* buffer, int len){
	int val = srtp_unprotect(*receive_session, buffer, &len)==0;
	if(val==0){
		return len;
	}else{
		printf("Error SRTP %u\n",val);
		return val;
	}
}
int SrtpChannel::ProtectRtcp(void* buffer, int len){
	int val = srtp_protect_rtcp(*receive_session, buffer, &len)==0;
	if(val==0){
		return len;
	}else{
		printf("Error SRTP %u\n",val);
		return val;
	}
}
int SrtpChannel::UnprotectRtcp(void* buffer, int len){
	int val = srtp_unprotect_rtcp(*receive_session, buffer, &len)==0;
	if(val==0){
		return len;
	}else{
		printf("Error SRTP %u\n",val);
		return val;
	}
}
bool SrtpChannel::configureSRTPsession(srtp_t *session, const char* key, enum Type type ){
	srtp_policy_t policy;
	memset(&policy, 0, sizeof(policy));

	//char aokey[100];
	// initialize libSRTP
	srtp_init();

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
	return res!=0? false:true;
}




