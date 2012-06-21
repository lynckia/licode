/*
 * WebRTCConnection.cpp
 */

#include <cstdio>

#include "WebRtcConnection.h"
#include "NiceConnection.h"

#include "SdpInfo.h"

namespace erizo {

WebRtcConnection::WebRtcConnection(bool standAlone) {

	video_ = 1;
	audio_ = 1;
	bundle_ = true;
	localVideoSsrc_ = 55543;
	localAudioSsrc_ = 44444;
	videoReceiver_ = NULL;
	audioReceiver_ = NULL;
	audioNice_ = NULL;
	videoNice_ = NULL;
	audioSrtp_ = NULL;
	videoSrtp_ = NULL;

	this->standAlone_ = standAlone;
	if (!bundle_) {
		if (video_) {
			videoNice_ = new NiceConnection(VIDEO_TYPE, "");
			videoNice_->setWebRtcConnection(this);
			videoSrtp_ = new SrtpChannel();
			CryptoInfo crytp;
			crytp.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
			crytp.mediaType = VIDEO_TYPE;
			std::string key = SrtpChannel::generateBase64Key();

			crytp.keyParams = key;
			crytp.tag = 0;
			localSdp_.addCrypto(crytp);
			localSdp_.videoSsrc = localVideoSsrc_;
		}

		if (audio_) {
			audioNice_ = new NiceConnection(AUDIO_TYPE, "");
			audioNice_->setWebRtcConnection(this);
			audioSrtp_ = new SrtpChannel();
			CryptoInfo crytp;
			crytp.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
			crytp.mediaType = AUDIO_TYPE;
			crytp.tag = 1;
			std::string key = SrtpChannel::generateBase64Key();
			crytp.keyParams = key;
			localSdp_.addCrypto(crytp);
			localSdp_.audioSsrc = localAudioSsrc_;
		}

	} else {
		videoNice_ = new NiceConnection(VIDEO_TYPE, "");
		videoNice_->setWebRtcConnection(this);
		videoSrtp_ = new SrtpChannel();
		CryptoInfo crytpv;
		crytpv.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
		crytpv.mediaType = VIDEO_TYPE;
		std::string keyv = SrtpChannel::generateBase64Key();
		crytpv.keyParams = keyv;
		crytpv.tag = 1;
		localSdp_.addCrypto(crytpv);
		localSdp_.videoSsrc = localVideoSsrc_;
//		audioSrtp_ = new SrtpChannel();
		CryptoInfo crytpa;
		crytpa.cipherSuite = std::string("AES_CM_128_HMAC_SHA1_80");
		crytpa.mediaType = AUDIO_TYPE;
		//crytpa.tag = 1;
		crytpa.tag = 1;
//		std::string keya = SrtpChannel::generateBase64Key();
		crytpa.keyParams = keyv;
		localSdp_.addCrypto(crytpa);
		localSdp_.audioSsrc = localAudioSsrc_;

	}

	printf("WebRTCConnection constructed with video %d audio %d\n", video_,
			audio_);
}

WebRtcConnection::~WebRtcConnection() {

	this->close();
}

bool WebRtcConnection::init() {

	std::vector<CandidateInfo> *cands;
	if (!bundle_) {
		if (video_) {
			videoNice_->start();
			while (videoNice_->iceState != NiceConnection::CANDIDATES_GATHERED) {
				sleep(1);
			}
			cands = videoNice_->localCandidates;
			for (unsigned int it = 0; it < cands->size(); it++) {
				CandidateInfo cand = cands->at(it);
				localSdp_.addCandidate(cand);
			}
		}
		if (audio_) {
			audioNice_->start();
			while (audioNice_->iceState != NiceConnection::CANDIDATES_GATHERED) {
				sleep(1);
			}
			cands = audioNice_->localCandidates;
			for (unsigned int it = 0; it < cands->size(); it++) {
				CandidateInfo cand = cands->at(it);
				localSdp_.addCandidate(cand);
			}
		}

	} else {
		videoNice_->start();
		while (videoNice_->iceState != NiceConnection::CANDIDATES_GATHERED) {
			sleep(1);
		}
		cands = videoNice_->localCandidates;
		for (unsigned int it = 0; it < cands->size(); it++) {
			CandidateInfo cand = cands->at(it);
			cand.isBundle = bundle_;
			localSdp_.addCandidate(cand);
			cand.mediaType = AUDIO_TYPE;
			localSdp_.addCandidate(cand);

		}
	}
	return true;
}

void WebRtcConnection::close() {

	if (audio_) {
		if (audioNice_ != NULL) {
			audioNice_->close();
			audioNice_->join();
			delete audioNice_;
		}
		if (audioSrtp_ != NULL)
			delete audioSrtp_;
	}
	if (video_) {
		if (videoNice_ != NULL) {
			videoNice_->close();
			videoNice_->join();
			delete videoNice_;
		}
		if (videoSrtp_ != NULL)
			delete videoSrtp_;
	}
}

bool WebRtcConnection::setRemoteSdp(const std::string &sdp) {

	remoteSdp_.initWithSdp(sdp);
	std::vector<CryptoInfo> crypto_remote = remoteSdp_.getCryptoInfos();
	std::vector<CryptoInfo> crypto_local = localSdp_.getCryptoInfos();
	video_ = false;
	audio_ = false;

	CryptoInfo cryptLocal_video;
	CryptoInfo cryptLocal_audio;
	CryptoInfo cryptRemote_video;
	CryptoInfo cryptRemote_audio;

	for (unsigned int it = 0; it < crypto_remote.size(); it++) {
		CryptoInfo cryptemp = crypto_remote[it];
		if (cryptemp.mediaType == VIDEO_TYPE
				&& !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
			video_ = true;
			cryptRemote_video = cryptemp;
		} else if (cryptemp.mediaType == AUDIO_TYPE
				&& !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
			audio_ = true;
			cryptRemote_audio = cryptemp;
		}
	}
	for (unsigned int it = 0; it < crypto_local.size(); it++) {
		CryptoInfo cryptemp = crypto_local[it];
		if (cryptemp.mediaType == VIDEO_TYPE
				&& !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
			cryptLocal_video = cryptemp;
		} else if (cryptemp.mediaType == AUDIO_TYPE
				&& !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
			cryptLocal_audio = cryptemp;
		}
	}
	if (!bundle_) {
		if (video_) {
			videoNice_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
			videoSrtp_->setRtpParams((char*) cryptLocal_video.keyParams.c_str(),
					(char*) cryptRemote_video.keyParams.c_str());

		}
		if (audio_) {
			audioNice_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
			audioSrtp_->setRtpParams((char*) cryptLocal_audio.keyParams.c_str(),
					(char*) cryptRemote_audio.keyParams.c_str());
		}
	} else {
		videoNice_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
		remoteVideoSSRC_ = remoteSdp_.videoSsrc;
		remoteAudioSSRC_ = remoteSdp_.audioSsrc;
		videoSrtp_->setRtpParams((char*) cryptLocal_video.keyParams.c_str(),
				(char*) cryptRemote_video.keyParams.c_str());
//		audioSrtp_->setRtpParams((char*)cryptLocal_audio.keyParams.c_str(), (char*)cryptRemote_audio.keyParams.c_str());

	}

	if (standAlone_) {
		if (audio_)
			audioNice_->join();
		else
			videoNice_->join();
	}

	return true;
}

std::string WebRtcConnection::getLocalSdp() {

	return localSdp_.getSdp();
}

void WebRtcConnection::setAudioReceiver(MediaReceiver *receiv) {

	this->audioReceiver_ = receiv;
}

void WebRtcConnection::setVideoReceiver(MediaReceiver *receiv) {

	this->videoReceiver_ = receiv;
}

int WebRtcConnection::receiveAudioData(char* buf, int len) {
	boost::mutex::scoped_lock lock(receiveAudioMutex_);
	int res = -1;
	int length = len;
	if (audioSrtp_) {
		audioSrtp_->protectRtp(buf, &length);
//		printf("A mandar %d\n", length);
	}
	if (len <= 0)
		return length;
	if (audioNice_) {
		res = audioNice_->sendData(buf, length);
	}
	return res;
}

int WebRtcConnection::receiveVideoData(char* buf, int len) {
	boost::mutex::scoped_lock lock(receiveVideoMutex_);
	int res = -1;
	int length = len;
	if (videoSrtp_) {
		videoSrtp_->protectRtp(buf, &length);
	}
	if (length <= 10)
		return length;
	if (videoNice_) {
		res = videoNice_->sendData(buf, length);
	}
	return res;
}

int WebRtcConnection::receiveNiceData(char* buf, int len,
		NiceConnection* nice) {
//	printf("Receive Nice Data %d, type %d\n", len, nice->mediaType);
	boost::mutex::scoped_lock lock(writeMutex_);
	if (audioReceiver_ == NULL && videoReceiver_ == NULL)
		return 0;

	int length = len;
	if (bundle_) {
		if (videoSrtp_) {
			videoSrtp_->unprotectRtp(buf, &length);
		}
		if (length <= 0)
			return length;
		rtpheader* inHead = (rtpheader*) buf;
		if (inHead->ssrc == htonl(remoteVideoSSRC_)) {
			inHead->ssrc = htonl(localVideoSsrc_);
			videoReceiver_->receiveVideoData(buf, length);

		} else if (inHead->ssrc == htonl(remoteAudioSSRC_)) {
			inHead->ssrc = htonl(localAudioSsrc_);
			videoReceiver_->receiveVideoData(buf, length); // We send it via the video nice, the only one we have
		} else {
			printf("Unknown SSRC, ignoring\n");
		}
		return length;

	}

	if (nice->mediaType == AUDIO_TYPE) {
		if (audioReceiver_ != NULL) {
			if (audioSrtp_) {
				audioSrtp_->unprotectRtp(buf, &length);
			}
			if (length <= 0)
				return length;
			rtpheader *head = (rtpheader*) buf;
			head->ssrc = htonl(localAudioSsrc_);
			audioReceiver_->receiveAudioData(buf, length);
			return length;
		}
	} else if (nice->mediaType == VIDEO_TYPE) {
		if (videoReceiver_ != NULL) {
			if (videoSrtp_) {
				videoSrtp_->unprotectRtp(buf, &length);
			}
			if (length <= 0)
				return length;
			rtpheader *head = (rtpheader*) buf;
			head->ssrc = htonl(localVideoSsrc_);
			videoReceiver_->receiveVideoData(buf, length);
			return length;
		}
	}
	return -1;
}

} /* namespace erizo */
