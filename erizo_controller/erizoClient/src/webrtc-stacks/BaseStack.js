/* global RTCSessionDescription, RTCIceCandidate, RTCPeerConnection */
// eslint-disable-next-line
import SemanticSdp from '../../../common/semanticSdp/SemanticSdp';

import SdpHelpers from '../utils/SdpHelpers';
import Logger from '../utils/Logger';

const BaseStack = (specInput) => {
  const that = {};
  const specBase = specInput;
  const offerQueue = [];
  let localDesc;
  let remoteDesc;
  let localSdp;
  let remoteSdp;
  let isNegotiating = false;
  let latestSessionVersion = -1;

  Logger.info('Starting Base stack', specBase);

  that.pcConfig = {
    iceServers: [],
  };

  that.con = {};
  if (specBase.iceServers !== undefined) {
    that.pcConfig.iceServers = specBase.iceServers;
  }
  if (specBase.forceTurn === true) {
    that.pcConfig.iceTransportPolicy = 'relay';
  }
  that.audio = specBase.audio;
  that.video = specBase.video;
  if (that.audio === undefined) {
    that.audio = true;
  }
  if (that.video === undefined) {
    that.video = true;
  }
  specBase.remoteCandidates = [];
  specBase.localCandidates = [];
  specBase.remoteDescriptionSet = false;

  that.mediaConstraints = {
    offerToReceiveVideo: (that.video !== undefined && that.video !== false),
    offerToReceiveAudio: (that.audio !== undefined && that.audio !== false),
  };

  that.peerConnection = new RTCPeerConnection(that.pcConfig, that.con);

  // Aux functions

  const errorCallback = (where, errorcb, message) => {
    Logger.error('message:', message, 'in baseStack at', where);
    if (errorcb !== undefined) {
      errorcb('error');
    }
  };

  const successCallback = (message) => {
    Logger.info('Success in BaseStack', message);
  };

  const onIceCandidate = (event) => {
    let candidateObject = {};
    const candidate = event.candidate;
    if (!candidate) {
      Logger.info('Gathered all candidates. Sending END candidate');
      candidateObject = {
        sdpMLineIndex: -1,
        sdpMid: 'end',
        candidate: 'end',
      };
    } else {
      if (!candidate.candidate.match(/a=/)) {
        candidate.candidate = `a=${candidate.candidate}`;
      }

      candidateObject = {
        sdpMLineIndex: candidate.sdpMLineIndex,
        sdpMid: candidate.sdpMid,
        candidate: candidate.candidate,
      };
    }

    if (specBase.remoteDescriptionSet) {
      specBase.callback({ type: 'candidate', candidate: candidateObject });
    } else {
      specBase.localCandidates.push(candidateObject);
      Logger.info('Storing candidate: ', specBase.localCandidates.length, candidateObject);
    }
  };

  const setLocalDescForOffer = (isSubscribe, streamId, sessionDescription) => {
    localDesc = sessionDescription;
    if (!isSubscribe) {
      localDesc.sdp = that.enableSimulcast(localDesc.sdp);
    }
    localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
    SdpHelpers.setMaxBW(localSdp, specBase);
    localDesc.sdp = localSdp.toString();
    that.localSdp = localSdp;

    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
      config: { maxVideoBW: specBase.maxVideoBW },
    }, streamId);
  };

  const setLocalDescForAnswerp2p = (sessionDescription) => {
    localDesc = sessionDescription;
    localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
    SdpHelpers.setMaxBW(localSdp, specBase);
    localDesc.sdp = localSdp.toString();
    that.localSdp = localSdp;
    specBase.callback({
      type: localDesc.type,
      sdp: localDesc.sdp,
    });
    Logger.info('Setting local description p2p', localDesc);
    that.peerConnection.setLocalDescription(localDesc).then(successCallback)
    .catch(errorCallback);
  };

  const processOffer = (message) => {
    // Its an offer, we assume its p2p
    const msg = message;
    remoteSdp = SemanticSdp.SDPInfo.processString(msg.sdp);
    SdpHelpers.setMaxBW(remoteSdp, specBase);
    msg.sdp = remoteSdp.toString();
    that.remoteSdp = remoteSdp;
    that.peerConnection.setRemoteDescription(msg).then(() => {
      that.peerConnection.createAnswer(that.mediaConstraints)
      .then(setLocalDescForAnswerp2p).catch(errorCallback.bind(null, 'createAnswer p2p', undefined));
      specBase.remoteDescriptionSet = true;
    }).catch(errorCallback.bind(null, 'process Offer', undefined));
  };

  const processAnswer = (message) => {
    const msg = message;

    remoteSdp = SemanticSdp.SDPInfo.processString(msg.sdp);
    const sessionVersion = remoteSdp && remoteSdp.origin && remoteSdp.origin.sessionVersion;
    if (latestSessionVersion >= sessionVersion) {
      return;
    }
    Logger.info('Set remote and local description');
    latestSessionVersion = sessionVersion;

    SdpHelpers.setMaxBW(remoteSdp, specBase);
    that.setStartVideoBW(remoteSdp);
    that.setHardMinVideoBW(remoteSdp);

    msg.sdp = remoteSdp.toString();
    Logger.debug('Remote Description', msg.sdp);
    Logger.debug('Local Description', localDesc.sdp);
    that.remoteSdp = remoteSdp;

    remoteDesc = msg;
    that.peerConnection.setLocalDescription(localDesc).then(() => {
      that.peerConnection.setRemoteDescription(new RTCSessionDescription(msg)).then(() => {
        specBase.remoteDescriptionSet = true;
        Logger.info('Candidates to be added: ', specBase.remoteCandidates.length,
                      specBase.remoteCandidates);
        while (specBase.remoteCandidates.length > 0) {
          // IMPORTANT: preserve ordering of candidates
          that.peerConnection.addIceCandidate(specBase.remoteCandidates.shift());
        }
        Logger.info('Local candidates to send:', specBase.localCandidates.length);
        while (specBase.localCandidates.length > 0) {
          // IMPORTANT: preserve ordering of candidates
          specBase.callback({ type: 'candidate', candidate: specBase.localCandidates.shift() });
        }
        isNegotiating = false;
        if (offerQueue.length > 0) {
          const args = offerQueue.pop();
          that.createOffer(args[0], args[1], args[2]);
        }
      }).catch(errorCallback.bind(null, 'processAnswer', undefined));
    }).catch(errorCallback.bind(null, 'processAnswer', undefined));
  };

  const processNewCandidate = (message) => {
    const msg = message;
    try {
      let obj;
      if (typeof (msg.candidate) === 'object') {
        obj = msg.candidate;
      } else {
        obj = JSON.parse(msg.candidate);
      }
      if (obj.candidate === 'end') {
        // ignore the end candidate for chrome
        return;
      }
      obj.candidate = obj.candidate.replace(/a=/g, '');
      obj.sdpMLineIndex = parseInt(obj.sdpMLineIndex, 10);
      const candidate = new RTCIceCandidate(obj);
      if (specBase.remoteDescriptionSet) {
        that.peerConnection.addIceCandidate(candidate);
      } else {
        specBase.remoteCandidates.push(candidate);
      }
    } catch (e) {
      Logger.error('Error parsing candidate', msg.candidate);
    }
  };

  // Peerconnection events

  that.peerConnection.onicecandidate = onIceCandidate;
  // public functions

  that.setStartVideoBW = (sdpInput) => {
    Logger.error('startVideoBW not implemented for this browser');
    return sdpInput;
  };

  that.setHardMinVideoBW = (sdpInput) => {
    Logger.error('hardMinVideoBw not implemented for this browser');
    return sdpInput;
  };

  that.enableSimulcast = (sdpInput) => {
    Logger.error('Simulcast not implemented');
    return sdpInput;
  };

  that.close = () => {
    that.state = 'closed';
    that.peerConnection.close();
  };

  that.setSimulcast = (enable) => {
    that.simulcast = enable;
  };

  that.setVideo = (video) => {
    that.video = video;
  };

  that.setAudio = (audio) => {
    that.audio = audio;
  };

  that.updateSpec = (configInput, streamId, callback = () => {}) => {
    const config = configInput;
    const shouldApplyMaxVideoBWToSdp = specBase.p2p && config.maxVideoBW;
    const shouldSendMaxVideoBWInOptions = !specBase.p2p && config.maxVideoBW;
    if (shouldApplyMaxVideoBWToSdp || config.maxAudioBW) {
      if (config.maxVideoBW) {
        Logger.debug('Maxvideo Requested:', config.maxVideoBW,
                                'limit:', specBase.limitMaxVideoBW);
        if (config.maxVideoBW > specBase.limitMaxVideoBW) {
          config.maxVideoBW = specBase.limitMaxVideoBW;
        }
        specBase.maxVideoBW = config.maxVideoBW;
        Logger.debug('Result', specBase.maxVideoBW);
      }
      if (config.maxAudioBW) {
        if (config.maxAudioBW > specBase.limitMaxAudioBW) {
          config.maxAudioBW = specBase.limitMaxAudioBW;
        }
        specBase.maxAudioBW = config.maxAudioBW;
      }

      localSdp = SemanticSdp.SDPInfo.processString(localDesc.sdp);
      SdpHelpers.setMaxBW(localSdp, specBase);
      localDesc.sdp = localSdp.toString();
      that.localSdp = localSdp;

      if (config.Sdp || config.maxAudioBW) {
        Logger.debug('Updating with SDP renegotiation', specBase.maxVideoBW, specBase.maxAudioBW);
        that.peerConnection.setLocalDescription(localDesc)
          .then(() => {
            remoteSdp = SemanticSdp.SDPInfo.processString(remoteDesc.sdp);
            SdpHelpers.setMaxBW(remoteSdp, specBase);
            remoteDesc.sdp = remoteSdp.toString();
            that.remoteSdp = remoteSdp;
            return that.peerConnection.setRemoteDescription(new RTCSessionDescription(remoteDesc));
          }).then(() => {
            specBase.remoteDescriptionSet = true;
            specBase.callback({ type: 'updatestream', sdp: localDesc.sdp }, streamId);
          }).catch(errorCallback.bind(null, 'updateSpec', callback));
      } else {
        Logger.debug('Updating without SDP renegotiation, ' +
                     'newVideoBW:', specBase.maxVideoBW,
                     'newAudioBW:', specBase.maxAudioBW);
        specBase.callback({ type: 'updatestream', sdp: localDesc.sdp }, streamId);
      }
    }
    if (shouldSendMaxVideoBWInOptions ||
        config.minVideoBW ||
        (config.slideShowMode !== undefined) ||
        (config.muteStream !== undefined) ||
        (config.qualityLayer !== undefined) ||
        (config.minLayer !== undefined) ||
        (config.video !== undefined)) {
      Logger.debug('MaxVideoBW Changed to ', config.maxVideoBW);
      Logger.debug('MinVideo Changed to ', config.minVideoBW);
      Logger.debug('SlideShowMode Changed to ', config.slideShowMode);
      Logger.debug('muteStream changed to ', config.muteStream);
      Logger.debug('Video Constraints', config.video);
      specBase.callback({ type: 'updatestream', config }, streamId);
    }
  };

  that.createOffer = (isSubscribe = false, forceOfferToReceive = false, streamId = '') => {
    if (!isSubscribe && !forceOfferToReceive) {
      that.mediaConstraints = {
        offerToReceiveVideo: false,
        offerToReceiveAudio: false,
      };
    }
    if (isNegotiating) {
      offerQueue.push([isSubscribe, forceOfferToReceive, streamId]);
      return;
    }
    isNegotiating = true;
    Logger.debug('Creating offer', that.mediaConstraints, streamId);
    that.peerConnection.createOffer(that.mediaConstraints)
    .then(setLocalDescForOffer.bind(null, isSubscribe, streamId))
    .catch(errorCallback.bind(null, 'Create Offer', undefined));
  };

  that.addStream = (stream) => {
    that.peerConnection.addStream(stream);
  };

  that.processSignalingMessage = (msgInput) => {
    if (msgInput.type === 'offer') {
      processOffer(msgInput);
    } else if (msgInput.type === 'answer') {
      processAnswer(msgInput);
    } else if (msgInput.type === 'candidate') {
      processNewCandidate(msgInput);
    }
  };

  return that;
};

export default BaseStack;
