/*
 * WebRTCConnection.cpp
 *
 *  Created on: Mar 14, 2012
 *      Author: pedro
 */

#include "WebRTCConnection.h"
#include "sdpinfo.h"

#include <cstdio>

#include <boost/thread.hpp>

WebRTCConnection::WebRTCConnection(bool standAlone): MediaReceiver() {
	// TODO Auto-generated constructor stub
	video = 1;
	audio = 1;
	video_ssrc = 55543;
	audio_ssrc = 44444;
	video_receiver = NULL;
	audio_receiver = NULL;
	audio_nice = NULL;
	video_nice = NULL;
	audio_nice_rtcp = NULL;
	video_nice_rtcp = NULL;
	audio_srtp = NULL;
	video_srtp = NULL;


	this->standAlone = standAlone;
	if(video){
		video_nice = new NiceConnection(VIDEO_TYPE,"");
//		video_nice_rtcp = new NiceConnection(VIDEO_TYPE,"video_rtcp");
		video_nice->setWebRTCConnection(this);
//		video_nice_rtcp->setWebRTCConnection(this);
		video_srtp = new SrtpChannel();
//		video_receiver = this;
		CryptoInfo crytp;
		crytp.cipher_suite=std::string("AES_CM_128_HMAC_SHA1_80");
		crytp.media_type= VIDEO_TYPE;
		std::string key = SrtpChannel::generateBase64Key();

		crytp.key_params = key;
		crytp.tag = 0;
//		crytp.key_params = std::string("b0c+3druTInEz+C4EH2widkWdh7u9McnEn8vy/JM");
		local_sdp.addCrypto(crytp);
		local_sdp.video_ssrc = video_ssrc;
	}
	if (audio){
		audio_nice = new NiceConnection(AUDIO_TYPE, "");
//		audio_nice_rtcp = new NiceConnection(AUDIO_TYPE, "rtcp");
		audio_nice->setWebRTCConnection(this);
//		audio_nice_rtcp->setWebRTCConnection(this);
		audio_srtp = new SrtpChannel();
//		audio_receiver = this;
		CryptoInfo crytp;
		crytp.cipher_suite=std::string("AES_CM_128_HMAC_SHA1_80");
		crytp.media_type= AUDIO_TYPE;
		crytp.tag = 1;
		std::string key = SrtpChannel::generateBase64Key();
		crytp.key_params = key;
//		crytp.key_params = std::string("b0c+3druTInEz+C4EH2widkWdh7u9McnEn8vy/JM");
		local_sdp.addCrypto(crytp);
		local_sdp.audio_ssrc = audio_ssrc;

	}
	printf("WebRTCConnection constructed with video %d audio %d\n", video, audio);
}

WebRTCConnection::~WebRTCConnection() {
	this->close();
//	if (video_nice!=NULL)
//		delete video_nice;
//	if (video_nice_rtcp)
//		delete video_nice_rtcp;
//	if(video_srtp!=NULL)
//		delete video_srtp;
//	if (audio_nice!=NULL)
//		delete audio_nice;
//	if (audio_nice_rtcp!=NULL)
//			delete audio_nice_rtcp;
//	if (audio_srtp!=NULL)
//			delete audio_srtp;

	// TODO Auto-generated destructor stub
}

bool WebRTCConnection::init(){
	std::vector<CandidateInfo> *cands;
	if (video){

		video_nice->start();
		sleep(1);
//		video_nice_rtcp->start();
//		sleep(1);

		while (video_nice->state!=NiceConnection::CANDIDATES_GATHERED){
			sleep(1);
		}


		cands = video_nice->local_candidates;

		for (unsigned int it = 0; it<cands->size();it++ ){
			CandidateInfo cand = cands->at(it);
			local_sdp.addCandidate(cand);
		}
//		while (video_nice_rtcp->state!=NiceConnection::CANDIDATES_GATHERED){
//			sleep(1);
//		}

//		cands = video_nice_rtcp->local_candidates;
//		for (unsigned int it = 0; it<cands->size();it++ ){
//			CandidateInfo cand = cands->at(it);
//			local_sdp.addCandidate(cand);
//		}

	}
	if (audio){
		audio_nice->start();
		sleep(1);
//		audio_nice_rtcp->start();
		sleep(1);
		while (audio_nice->state!=NiceConnection::CANDIDATES_GATHERED){
			sleep(1);
		}

		cands = audio_nice->local_candidates;

		for (unsigned int it = 0; it<cands->size();it++ ){
			CandidateInfo cand = cands->at(it);
			local_sdp.addCandidate(cand);
		}
//		while (audio_nice_rtcp->state!=NiceConnection::CANDIDATES_GATHERED){
//			sleep(1);
//		}

//		cands = audio_nice_rtcp->local_candidates;
//		for (unsigned int it = 0; it<cands->size();it++ ){
//			CandidateInfo cand = cands->at(it);
//			local_sdp.addCandidate(cand);
//		}

	}

	return true;
}

void WebRTCConnection::close(){
	if (audio_nice!=NULL){
		audio_nice->close();
		audio_nice->join();
		delete audio_nice;
	}
	if (audio_nice_rtcp!=NULL){
		audio_nice_rtcp->close();
		audio_nice_rtcp->join();
		delete audio_nice_rtcp;
	}
	if (video_nice!=NULL){
		video_nice->close();
		video_nice->join();
		delete video_nice;
	}
	if (video_nice_rtcp!=NULL){
		video_nice_rtcp->close();
		video_nice_rtcp->join();
		delete video_nice_rtcp;
	}
	if (audio_srtp!=NULL)
		delete audio_srtp;
	if (video_srtp!=NULL)
		delete video_srtp;
}
bool WebRTCConnection::setRemoteSDP(const std::string &sdp){
	remote_sdp.initWithSDP(sdp);
	std::vector<CryptoInfo> crypto_remote = remote_sdp.getCryptoInfos();
	std::vector<CryptoInfo> crypto_local = local_sdp.getCryptoInfos();

	CryptoInfo cryptLocal_video;
	CryptoInfo cryptLocal_audio;
	CryptoInfo cryptRemote_video;
	CryptoInfo cryptRemote_audio;

	for (unsigned int it = 0; it < crypto_remote.size(); it++){
		CryptoInfo cryptemp = crypto_remote[it];
		if (cryptemp.media_type == VIDEO_TYPE && !cryptemp.cipher_suite.compare("AES_CM_128_HMAC_SHA1_80")){
			video = true;
			cryptRemote_video = cryptemp;
		}else if (cryptemp.media_type == AUDIO_TYPE && !cryptemp.cipher_suite.compare("AES_CM_128_HMAC_SHA1_80")){
			audio = true;
			cryptRemote_audio = cryptemp;
		}
	}
	for (unsigned int it = 0; it < crypto_local.size(); it++){
		CryptoInfo cryptemp = crypto_local[it];
		if (cryptemp.media_type == VIDEO_TYPE && !cryptemp.cipher_suite.compare("AES_CM_128_HMAC_SHA1_80")){
			cryptLocal_video = cryptemp;
		}else if (cryptemp.media_type == AUDIO_TYPE && !cryptemp.cipher_suite.compare("AES_CM_128_HMAC_SHA1_80")){
			cryptLocal_audio = cryptemp;
		}
	}

	if(video){
		video_nice->setRemoteCandidates(remote_sdp.getCandidateInfos());
//		video_nice_rtcp->setRemoteCandidates(remote_sdp.getCandidateInfos());
		video_srtp->SetRtpParams((char*)cryptLocal_video.key_params.c_str(), (char*)cryptRemote_video.key_params.c_str());

	}
	if (audio){
		audio_nice->setRemoteCandidates(remote_sdp.getCandidateInfos());
//		audio_nice_rtcp->setRemoteCandidates(remote_sdp.getCandidateInfos());

		audio_srtp->SetRtpParams((char*)cryptLocal_audio.key_params.c_str(), (char*)cryptRemote_audio.key_params.c_str());
	}
	if (standAlone){
		if (audio)
			audio_nice->join();
		else
			video_nice->join();
	}

	return true;
}
std::string WebRTCConnection::getLocalSDP(){
	return local_sdp.getSDP();
}
void WebRTCConnection::setAudioReceiver(MediaReceiver *receiv){
	this->audio_receiver = receiv;
}
void WebRTCConnection::setVideoReceiver(MediaReceiver *receiv){
	this->video_receiver = receiv;
}

int WebRTCConnection::receiveAudioData(char* buf, int len){
	boost::mutex::scoped_lock lock(receiveAudio);

	int res=-1;
	int length= len;
	if (audio_srtp){
		audio_srtp->ProtectRtp(buf, &length);
//		printf("A mandar %d\n", length);
	}
	if (len<=0)
			return length;
	if (audio_nice){
		res= audio_nice->sendData(buf, length);
	}
	return res;
}
int WebRTCConnection::receiveVideoData(char* buf, int len){
	boost::mutex::scoped_lock lock(receiveVideo);

	int res=-1;
	int length= len;
	if (video_srtp){
		video_srtp->ProtectRtp(buf, &length);
//		printf("A mandar %d\n", length);
	}
	if (length<=10)
		return length;
	if (video_nice){
		res= video_nice->sendData(buf, length);
	}
	return res;
}

int WebRTCConnection::receiveNiceData(char* buf, int len, NiceConnection* nice){
	boost::mutex::scoped_lock lock(write_mutex);
	if (audio_receiver == NULL && video_receiver == NULL)
		return 0;

	int length = len;

	if(nice->media_type == AUDIO_TYPE){
		if (audio_receiver!=NULL){
			if (audio_srtp){
//				printf("por aqui %d\n", length);
				audio_srtp->UnprotectRtp(buf,&length);
//				printf("por desprotegido audio %d\n", length);
			}
			if(length<=0)
				return length;
			rtpheader *head = (rtpheader*) buf;
			head->ssrc = htonl(audio_ssrc);
			audio_receiver->receiveAudioData(buf, length);
			return length;
		}
	}
	else if(nice->media_type == VIDEO_TYPE){
//		printf("RECIBIDO VIDEO %d\n", length);

		if (video_receiver!=NULL){
			if (video_srtp){
				video_srtp->UnprotectRtp(buf,&length);
//				printf("por desprotegido video %d\n", length);

			}
			if(length<=0)
				return length;
			rtpheader *head = (rtpheader*) buf;
			head->ssrc = htonl(video_ssrc);
			video_receiver->receiveVideoData(buf, length);
			return length;
		}
	}
	return -1;
}




